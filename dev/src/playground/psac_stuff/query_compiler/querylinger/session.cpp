#include "session.hpp"

#include "queries/codegen.hpp"
#include "queries/inputs.hpp"
#include "session_state.hpp"

#include <psackling/pipeline.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <hashing/hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {
	ql::SessionState* active_session = nullptr;

	/// Object files are keyed by the link-name hash (not the work-item position like
	/// psacklinger's .fnN.o), so they stay stable when functions are reordered,
	/// added or removed.
	std::string objectFileTag(const std::string& link_name) {
		return hashing::justHash<hashing::SHA256>(link_name).toStringHex().substr(0, 32);
	}

	void writeFileBytes(const std::filesystem::path& path, const std::string& bytes) {
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) throw std::runtime_error("Unable to create file: " + path.string());
		out.write(bytes.data(), std::streamsize(bytes.size()));
		if (!out) throw std::runtime_error("Failed to write file: " + path.string());
	}
}

namespace ql {
	const SessionState& activeSession() {
		CORE_ASSERT(active_session != nullptr, "No active querylinger session");
		return *active_session;
	}

	struct Compiler::State {
		SessionState session;

		CompileOptions options;

		std::vector<std::filesystem::path> object_paths;
		std::filesystem::path              globals_object_path;

		/// Side-input set of the previous compile; diffed against the new set by
		/// invalidateQueries (the psac_write + psac_propagate analog).
		std::vector<query::external::InputData> previous_inputs;

		std::vector<std::string> previous_link_names;

		/// SHA-256 of the object bytes last written per link name (plus the globals
		/// object), so unchanged cache-hit results are not rewritten to disk.
		base::HashMap<std::string, base::Bit256> written_objects;
		base::Optional<base::Bit256>             written_globals_object;

		bool compiled_once = false;
	};

	namespace {
		void installProgram(Compiler::State& state, pg::AnalyzedProgram program) {
			state.session.program            = std::move(program);
			state.session.index_by_link_name = {};
			state.object_paths.clear();

			for (usize i = 0; i < state.session.program.link_names.size(); ++i) {
				const std::string& link_name = state.session.program.link_names[i];
				state.session.index_by_link_name.insertOrAssign(link_name, i);

				auto object_path = std::filesystem::path(state.options.output_path);
				object_path += ".fn_" + objectFileTag(link_name) + ".o";
				state.object_paths.push_back(std::move(object_path));
			}

			state.globals_object_path = std::filesystem::path(state.options.output_path);
			state.globals_object_path += ".globals.o";
		}

		std::vector<query::external::InputData> collectInputs(const SessionState& session) {
			const pg::AnalyzedProgram& program = session.program;

			std::vector<query::external::InputData> inputs;
			inputs.reserve(
				program.items.size() + program.const_inputs.size() + program.sig_inputs.size()
				+ program.class_inputs.size() + program.global_var_inputs.size() + 2
			);

			auto add = [&inputs]<typename Input>(std::in_place_type_t<Input>, base::Bit256 hash) {
				const auto node_id = query::internal::makeNodeID<Input>(HashInputKey{ hash });
				inputs.emplace_back(Input::getID(), node_id.hash.val);
			};

			for (usize i = 0; i < program.items.size(); ++i)
				add(std::in_place_type<FnBodyInput>,
				    fnBodyInputHash(program.link_names[i], program.fn_hashes[i]));

			for (const pg::SymbolInputInfo& input: program.const_inputs)
				add(std::in_place_type<ConstInput>, input.input_hash);
			for (const pg::SymbolInputInfo& input: program.sig_inputs)
				add(std::in_place_type<SigInput>, input.input_hash);
			for (const pg::SymbolInputInfo& input: program.class_inputs)
				add(std::in_place_type<ClassInput>, input.input_hash);
			for (const pg::SymbolInputInfo& input: program.global_var_inputs)
				add(std::in_place_type<GlobalVarInput>, input.input_hash);

			add(std::in_place_type<GlobalDefsInput>, program.global_defs_hash);
			add(std::in_place_type<GlobalsInput>, program.globals_hash_no_consts);
			add(std::in_place_type<WorldInput>, program.globals_hash);
			return inputs;
		}
	}

	Compiler::Compiler(CompileOptions options): state(std::make_unique<State>()) {
		state->options = std::move(options);

		// Worker count is process-global and can be set only once; the embedder (CLI
		// main or the test suite) owns it. num_threads in the options is informational.
		query::setTrackReverseGraph(true);

		CORE_ASSERT(active_session == nullptr, "Only one querylinger session may be active");
		active_session = &state->session;

		// The query graph is process-global; drop every input node a previous session
		// may have left behind (with their dependents), so this session starts from a
		// clean slate instead of sharing nodes with an unrelated program.
		query::external::invalidateQueries({});
	}

	Compiler::~Compiler() { active_session = nullptr; }

	/// Schedules the globals object and every function object, awaits all results and
	/// links. Only invalidated (or new) queries actually execute; the rest are cache
	/// hits, so the recompile counter reports exactly the incremental work done. The
	/// providers only produce bytes; writing the object files (skipped for unchanged
	/// bytes) and linking happen here, outside the query graph.
	CompileStats Compiler::runBackendAndLink(bool allow_skip_link) {
		SessionState& session = state->session;
		const usize   n       = session.program.items.size();

		session.counters.reset();

		auto globals_handle = query::scheduleEntryPoint<QueryGlobalsObject>(GlobalsObjectKey{});

		std::vector<query::EntryTaskHandle> handles;
		handles.reserve(n);
		for (usize i = 0; i < n; ++i) {
			handles.push_back(query::scheduleEntryPoint<QueryFunctionObject>(FnObjectKey{
				.link_name = session.program.link_names[i] }));
		}

		const ObjectResult globals_result
			= query::awaitEntryPoint<QueryGlobalsObject>(globals_handle);
		if (!globals_result.error.empty()) throw std::runtime_error(globals_result.error);

		std::vector<ObjectResult> results;
		results.reserve(n);
		for (usize i = 0; i < n; ++i) {
			results.push_back(query::awaitEntryPoint<QueryFunctionObject>(handles[i]));
			if (!results.back().error.empty()) {
				throw std::runtime_error(
					"Compilation of function '" + session.program.link_names[i]
					+ "' failed: " + results.back().error
				);
			}
		}

		const base::Bit256 globals_bytes_hash
			= hashing::justHash<hashing::SHA256>(globals_result.object_bytes);
		if (state->written_globals_object != globals_bytes_hash) {
			writeFileBytes(state->globals_object_path, globals_result.object_bytes);
			state->written_globals_object = globals_bytes_hash;
		}

		for (usize i = 0; i < n; ++i) {
			const std::string& link_name = session.program.link_names[i];
			const base::Bit256 bytes_hash
				= hashing::justHash<hashing::SHA256>(results[i].object_bytes);
			if (state->written_objects.atMaybeCopy(link_name) == bytes_hash) continue;
			writeFileBytes(state->object_paths[i], results[i].object_bytes);
			state->written_objects.insertOrAssign(link_name, bytes_hash);
		}

		const CompileStats stats{ .total_functions = n,
			                      .recompiled_functions
			                      = session.counters.recompiled_functions.load() };

		// Nothing recompiled (functions or globals object) and the function set is
		// unchanged: the executable on disk is already current (psacklinger's no-op
		// update path skips the link too).
		const bool nothing_changed = stats.recompiled_functions == 0
		                          && session.counters.reemitted_globals_objects.load() == 0
		                          && session.program.link_names == state->previous_link_names;
		if (!(allow_skip_link && nothing_changed)) {
			pg::PhaseTimer timer("link");
			pg::linkObjects(
				state->object_paths, state->globals_object_path, state->options.output_path
			);
		}

		state->previous_link_names = session.program.link_names;
		return stats;
	}

	CompileStats Compiler::compile(const fs::File& file) {
		installProgram(*state, pg::analyzeProgram(file));

		auto new_inputs = collectInputs(state->session);
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

		installProgram(*state, pg::analyzeProgram(file));

		auto new_inputs      = collectInputs(state->session);
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
