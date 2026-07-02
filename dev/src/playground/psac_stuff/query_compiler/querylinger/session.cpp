#include "session.hpp"

#include "queries/codegen.hpp"
#include "queries/inputs.hpp"
#include "session_state.hpp"

#include <psackling/pipeline.hpp>

#include <base/except/exceptions.hpp>

#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <atomic>

namespace {
	std::atomic<u64> next_session_epoch{ 1 };

	ql::SessionState* active_session = nullptr;

	u64 linkNameHash(const std::string& name) { return std::hash<std::string>{}(name); }
}

namespace ql {
	SessionState& activeSession() {
		CORE_ASSERT(active_session != nullptr, "No active querylinger session");
		return *active_session;
	}

	struct Compiler::State {
		SessionState session;

		/// Side-input set of the previous compile; diffed against the new set by
		/// invalidateQueries (the psac_write + psac_propagate analog).
		std::vector<query::external::InputData> previous_inputs;

		std::vector<std::string> previous_link_names;

		bool compiled_once = false;
	};

	namespace {
		void installProgram(SessionState& session, pg::AnalyzedProgram program) {
			session.program            = std::move(program);
			session.index_by_link_name = {};
			session.object_paths.clear();

			for (usize i = 0; i < session.program.link_names.size(); ++i) {
				const std::string& link_name = session.program.link_names[i];
				session.index_by_link_name.insertOrAssign(link_name, i);

				// Object files are keyed by the link-name hash (not the work-item
				// position like psacklinger's .fnN.o), so they stay stable when
				// functions are reordered, added or removed.
				auto object_path = std::filesystem::path(session.options.output_path);
				object_path += ".fn_" + std::to_string(linkNameHash(link_name)) + ".o";
				session.object_paths.push_back(std::move(object_path));
			}

			session.globals_object_path = std::filesystem::path(session.options.output_path);
			session.globals_object_path += ".globals.o";
		}

		std::vector<query::external::InputData> collectInputs(const SessionState& session) {
			std::vector<query::external::InputData> inputs;
			inputs.reserve(session.program.items.size() + session.program.const_inputs.size() + 1);

			for (usize i = 0; i < session.program.items.size(); ++i) {
				const auto node_id = query::internal::makeNodeID<FnBodyInput>(FnBodyInputKey{
					.session_epoch = session.epoch,
					.link_hash     = linkNameHash(session.program.link_names[i]),
					.body_hash     = session.program.fn_hashes[i] });
				inputs.emplace_back(FnBodyInput::getID(), node_id.hash.val);
			}

			for (const pg::SymbolInputInfo& const_input: session.program.const_inputs) {
				const auto node_id = query::internal::makeNodeID<ConstInput>(ConstInputKey{
					.session_epoch = session.epoch,
					.name_hash     = const_input.name_hash,
					.value_hash    = const_input.value_hash });
				inputs.emplace_back(ConstInput::getID(), node_id.hash.val);
			}

			for (const pg::SymbolInputInfo& sig_input: session.program.sig_inputs) {
				const auto node_id = query::internal::makeNodeID<SigInput>(SigInputKey{
					.session_epoch = session.epoch,
					.name_hash     = sig_input.name_hash,
					.value_hash    = sig_input.value_hash });
				inputs.emplace_back(SigInput::getID(), node_id.hash.val);
			}

			for (const pg::SymbolInputInfo& class_input: session.program.class_inputs) {
				const auto node_id = query::internal::makeNodeID<ClassInput>(ClassInputKey{
					.session_epoch = session.epoch,
					.name_hash     = class_input.name_hash,
					.value_hash    = class_input.value_hash });
				inputs.emplace_back(ClassInput::getID(), node_id.hash.val);
			}

			for (const pg::SymbolInputInfo& var_input: session.program.global_var_inputs) {
				const auto node_id = query::internal::makeNodeID<GlobalVarInput>(GlobalVarInputKey{
					.session_epoch = session.epoch,
					.name_hash     = var_input.name_hash,
					.value_hash    = var_input.value_hash });
				inputs.emplace_back(GlobalVarInput::getID(), node_id.hash.val);
			}

			const auto defs_node_id
				= query::internal::makeNodeID<GlobalDefsInput>(GlobalDefsInputKey{
					.session_epoch = session.epoch, .defs_hash = session.program.global_defs_hash });
			inputs.emplace_back(GlobalDefsInput::getID(), defs_node_id.hash.val);

			const auto globals_node_id = query::internal::makeNodeID<GlobalsInput>(GlobalsInputKey{
				.session_epoch = session.epoch,
				.globals_hash  = session.program.globals_hash_no_consts });
			inputs.emplace_back(GlobalsInput::getID(), globals_node_id.hash.val);
			return inputs;
		}
	}

	Compiler::Compiler(CompileOptions options): state(std::make_unique<State>()) {
		state->session.epoch   = next_session_epoch.fetch_add(1, std::memory_order_relaxed);
		state->session.options = std::move(options);

		// Worker count is process-global and can be set only once; the embedder (CLI
		// main or the test suite) owns it. num_threads in the options is informational.
		query::setTrackReverseGraph(true);

		CORE_ASSERT(active_session == nullptr, "Only one querylinger session may be active");
		active_session = &state->session;
	}

	Compiler::~Compiler() { active_session = nullptr; }

	/// Schedules the globals object and every function object, awaits all results and
	/// links. Only invalidated (or new) queries actually execute; the rest are cache
	/// hits, so the recompile counter reports exactly the incremental work done.
	CompileStats Compiler::runBackendAndLink(bool allow_skip_link) {
		SessionState& session = state->session;
		const usize   n       = session.program.items.size();

		resetRecompileCounts();

		auto globals_handle = query::scheduleEntryPoint<QueryGlobalsObject>(GlobalsObjectKey{
			.session_epoch = session.epoch });

		std::vector<query::EntryTaskHandle> handles;
		handles.reserve(n);
		for (usize i = 0; i < n; ++i) {
			handles.push_back(query::scheduleEntryPoint<QueryFunctionObject>(FnObjectKey{
				.session_epoch = session.epoch,
				.link_hash     = linkNameHash(session.program.link_names[i]),
				.link_name     = session.program.link_names[i] }));
		}

		const ObjectResult globals_result
			= query::awaitEntryPoint<QueryGlobalsObject>(globals_handle);
		if (!globals_result.error.empty()) throw std::runtime_error(globals_result.error);

		for (usize i = 0; i < n; ++i) {
			const ObjectResult result = query::awaitEntryPoint<QueryFunctionObject>(handles[i]);
			if (!result.error.empty()) {
				throw std::runtime_error(
					"Compilation of function '" + session.program.link_names[i]
					+ "' failed: " + result.error
				);
			}
		}

		const CompileStats stats{ .total_functions      = n,
			                      .recompiled_functions = functionRecompileCount() };

		// Nothing recompiled (functions or globals object) and the function set is
		// unchanged: the executable on disk is already current (psacklinger's no-op
		// update path skips the link too).
		const bool nothing_changed = stats.recompiled_functions == 0
		                          && globalsObjectRecompileCount() == 0
		                          && session.program.link_names == state->previous_link_names;
		if (!(allow_skip_link && nothing_changed)) {
			pg::PhaseTimer timer("link");
			pg::linkObjects(
				session.object_paths, session.globals_object_path, session.options.output_path
			);
		}

		state->previous_link_names = session.program.link_names;
		return stats;
	}

	CompileStats Compiler::compile(const fs::File& file) {
		SessionState& session = state->session;

		installProgram(session, pg::analyzeProgram(file));

		auto new_inputs = collectInputs(session);
		if (state->compiled_once) {
			auto new_inputs_copy = new_inputs;
			query::external::invalidateQueries(
				std::move(new_inputs_copy), std::move(state->previous_inputs)
			);
		}
		state->previous_inputs = std::move(new_inputs);
		state->compiled_once   = true;

		return runBackendAndLink(/*allow_skip_link=*/false);
	}

	CompileStats Compiler::update(const fs::File& file) {
		if (!state->compiled_once) return compile(file);

		SessionState& session = state->session;

		installProgram(session, pg::analyzeProgram(file));

		auto new_inputs      = collectInputs(session);
		auto new_inputs_copy = new_inputs;
		query::external::invalidateQueries(
			std::move(new_inputs_copy), std::move(state->previous_inputs)
		);
		state->previous_inputs = std::move(new_inputs);

		return runBackendAndLink(/*allow_skip_link=*/true);
	}
}

void ql::compile(const fs::File& file, const CompileOptions& options) {
	CORE_DEV_LOG(
		Compiler,
		"Starting query-based compilation of ",
		file.getFilePath().string(),
		" to ",
		options.output_path,
		'\n'
	);

	Compiler compiler(options);
	compiler.compile(file);
}
