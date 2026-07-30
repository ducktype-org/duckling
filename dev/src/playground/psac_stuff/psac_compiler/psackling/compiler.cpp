#include "compiler.hpp"

#include "context.hpp"
#include "pipeline.hpp"

#include <frontend/pst_parser/pst.hpp>
#include <psac/psac.hpp>

#include <logger/logger.hpp>

#include <atomic>
#include <optional>
#include <thread>

namespace {
	/// parlay's scheduler is created during static initialization (a global
	/// block_allocator calls parallel_for) and its idle workers busy-spin, roughly
	/// doubling the serial front-end time through core/cache contention. Keep the pool
	/// parked at a single worker except around the parallel lowering phase.
	void parkScheduler() { parlay::set_num_workers(1); }

	void unparkScheduler(usize num_threads) {
		parlay::set_num_workers(
			num_threads > 0 ? num_threads : usize(std::thread::hardware_concurrency())
		);
	}

	/**
	 * @brief The PSAC computation: every function's compilation is a read of its
	 * content-hash Mod plus the shared globals-hash Mod, so psac_propagate re-runs it
	 * exactly when one of those was rewritten. Granularity 1 makes each function an
	 * independent parallel task. Errors are collected per-task (exceptions must not
	 * unwind into the scheduler).
	 */
	psac_function(
		compileFunctionsPsac,
		const pg::Context*                        base_ctx,
		const std::vector<pg::FunctionWorkItem>*  items,
		const std::vector<std::filesystem::path>* object_paths,
		std::vector<psac::Mod<base::Bit256>>*     fn_mods,
		psac::Mod<base::Bit256>*                  globals_mod,
		std::vector<std::string>*                 errors,
		std::atomic<usize>*                       recompile_count
	) {
		psac_parallel_for(u64 i, u64{ 0 }, u64{ items->size() }, 1, {
			psac_read(
				(base::Bit256 fn_hash, base::Bit256 globals_hash),
				(&(*fn_mods)[i], globals_mod),
				{
					(void) fn_hash;
					(void) globals_hash;
					recompile_count->fetch_add(1, std::memory_order_relaxed);
					try {
						pg::compileOneFunction(*base_ctx, (*items)[i], (*object_paths)[i]);
					} catch (const std::exception& e) { (*errors)[i] = e.what(); }
				}
			);
		});
	}
}

namespace pg {
	struct Compiler::State {
		CompileOptions options;

		// The PSTs must outlive the work items (they hold Access references into them).
		std::vector<LoadedModule>     modules;
		Context                       ctx;
		std::vector<FunctionWorkItem> items;
		std::vector<base::Bit256>     fn_hashes;
		std::vector<std::string>      link_names;
		std::vector<GlobalInitItem>   global_inits;
		base::Bit256                  globals_hash;

		std::vector<std::filesystem::path> object_paths;
		std::filesystem::path              globals_object_path;

		// PSAC inputs/outputs. The read closures capture raw pointers to all of these,
		// so they live here (stable addresses) and are only replaced after the
		// computation that references them is destroyed.
		std::vector<psac::Mod<base::Bit256>> fn_mods;
		psac::Mod<base::Bit256>              globals_mod;
		std::vector<std::string>             errors;
		std::atomic<usize>                   recompile_count{ 0 };
		std::optional<psac::Computation>     computation;
	};

	Compiler::Compiler(CompileOptions options): state(std::make_unique<State>()) {
		state->options = std::move(options);
		parkScheduler();
	}

	Compiler::~Compiler() = default;

	AnalyzedProgram Compiler::analyze(const fs::File& file) { return analyzeProgram(file); }

	void Compiler::throwOnErrors() const {
		for (usize i = 0; i < state->errors.size(); ++i) {
			if (!state->errors[i].empty()) {
				throw std::runtime_error(
					"Compilation of function '" + state->link_names[i]
					+ "' failed: " + state->errors[i]
				);
			}
		}
	}

	void Compiler::link() const {
		PhaseTimer timer("link");
		linkObjects(state->object_paths, state->globals_object_path, state->options.output_path);
	}

	CompileStats Compiler::compileAnalyzed(AnalyzedProgram program) {
		State& s = *state;

		// The previous computation holds pointers into the Mod vector; drop it before
		// replacing anything it references.
		s.computation.reset();
		psac::GarbageCollector::run();

		const usize n = program.items.size();

		s.modules      = std::move(program.modules);
		s.ctx          = program.ctx;
		s.items        = std::move(program.items);
		s.fn_hashes    = std::move(program.fn_hashes);
		s.link_names   = std::move(program.link_names);
		s.global_inits = std::move(program.global_inits);
		s.globals_hash = program.globals_hash;

		s.object_paths.clear();
		for (usize i = 0; i < n; ++i) {
			auto object_path = std::filesystem::path(s.options.output_path);
			object_path += ".fn" + std::to_string(i) + ".o";
			s.object_paths.push_back(std::move(object_path));
		}

		s.globals_object_path = std::filesystem::path(s.options.output_path);
		s.globals_object_path += ".globals.o";
		{
			PhaseTimer timer("globals_object");
			emitGlobalsObject(s.ctx, s.global_inits, s.globals_object_path);
		}

		s.fn_mods = std::vector<psac::Mod<base::Bit256>>(n);
		for (usize i = 0; i < n; ++i) psac_write(&s.fn_mods[i], s.fn_hashes[i]);
		psac_write(&s.globals_mod, s.globals_hash);

		s.errors.assign(n, "");
		s.recompile_count.store(0);

		{
			PhaseTimer timer("function_lowering");
			if (s.options.sequential) {
				for (usize i = 0; i < n; ++i)
					compileOneFunction(s.ctx, s.items[i], s.object_paths[i]);
			} else if (n > 0) {
				unparkScheduler(s.options.num_threads);
				// psac_run expands to a statement (trailing semicolon), so it can't be nested
				// inside a call expression.
				auto computation = psac_run(
									   compileFunctionsPsac,
									   &s.ctx,
									   &s.items,
									   &s.object_paths,
									   &s.fn_mods,
									   &s.globals_mod,
									   &s.errors,
									   &s.recompile_count
				) s.computation.emplace(std::move(computation));
				parkScheduler();
			}
		}

		throwOnErrors();
		link();
		return CompileStats{ .total_functions = n, .recompiled_functions = n };
	}

	CompileStats Compiler::compile(const fs::File& file) { return compileAnalyzed(analyze(file)); }

	CompileStats Compiler::update(const fs::File& file) {
		State& s = *state;

		AnalyzedProgram program = analyze(file);

		// No previous compilation, or the set of functions changed (added/removed/
		// renamed): the computation shape no longer matches, do a full build.
		const bool same_function_set = !s.modules.empty() && (program.link_names == s.link_names);
		if (!same_function_set) return compileAnalyzed(std::move(program));

		const usize n = s.items.size();

		// Refresh shared state in place — the PSAC read closures point at these. The
		// old PSTs are destroyed only after the items referencing them were replaced.
		s.ctx          = program.ctx;
		s.items        = std::move(program.items);
		s.global_inits = std::move(program.global_inits);
		s.modules      = std::move(program.modules);

		// Diff hashes and rewrite exactly the Mods that changed; propagation then
		// re-runs exactly the reads of those Mods.
		std::vector<usize> changed;
		for (usize i = 0; i < n; ++i) {
			if (program.fn_hashes[i] != s.fn_hashes[i]) {
				changed.push_back(i);
				if (!s.options.sequential) psac_write(&s.fn_mods[i], program.fn_hashes[i]);
			}
		}
		const bool globals_changed = program.globals_hash != s.globals_hash;
		if (globals_changed) {
			emitGlobalsObject(s.ctx, s.global_inits, s.globals_object_path);
			if (!s.options.sequential) psac_write(&s.globals_mod, program.globals_hash);
		}

		s.fn_hashes    = std::move(program.fn_hashes);
		s.globals_hash = program.globals_hash;

		if (changed.empty() && !globals_changed)
			return CompileStats{ .total_functions = n, .recompiled_functions = 0 };

		s.errors.assign(n, "");
		s.recompile_count.store(0);

		if (s.options.sequential) {
			if (globals_changed) {
				for (usize i = 0; i < n; ++i)
					compileOneFunction(s.ctx, s.items[i], s.object_paths[i]);
				s.recompile_count.store(n);
			} else {
				for (usize i: changed) compileOneFunction(s.ctx, s.items[i], s.object_paths[i]);
				s.recompile_count.store(changed.size());
			}
		} else {
			unparkScheduler(s.options.num_threads);
			psac_propagate(s.computation.value());
			psac::GarbageCollector::run();
			parkScheduler();
		}

		throwOnErrors();
		link();
		return CompileStats{ .total_functions      = n,
			                 .recompiled_functions = s.recompile_count.load() };
	}
}

void pg::compile(const fs::File& file, const CompileOptions& options) {
	CORE_DEV_LOG(
		Compiler,
		"Starting compilation process of ",
		file.getFilePath().string(),
		" to ",
		options.output_path,
		'\n'
	);

	Compiler compiler(options);
	compiler.compile(file);
}
