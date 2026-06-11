#pragma once

#include <filesystem/file.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace pg {
	struct CompileOptions {
		std::string output_path = "psacklinged.out";

		/// Number of PSAC/parlay worker threads; 0 means the scheduler default
		/// (the PARLAY_NUM_THREADS environment variable, or hardware concurrency).
		usize num_threads = 0;

		/// Compile functions one-by-one on the main thread instead of through the
		/// PSAC computation. Baseline for benchmarking the parallel pipeline.
		bool sequential = false;
	};

	struct CompileStats {
		usize total_functions      = 0;
		usize recompiled_functions = 0;
	};

	/**
	 * @brief Compiler session that keeps the PSAC computation alive between compiles,
	 * so feeding it new source recompiles only the functions whose content (or the
	 * global declarations they depend on) actually changed.
	 *
	 * Mirrors the real compiler's incremental design in miniature: there, queries
	 * depend on per-PST-element stable hashes collected as side inputs and are
	 * invalidated when those hashes change; here, every function's compilation is a
	 * PSAC read of (function content hash, globals hash) Mods, and psac_propagate
	 * re-runs exactly the reads whose Mods were rewritten.
	 */
	class Compiler final {
	public:
		explicit Compiler(CompileOptions options);
		~Compiler();

		Compiler(const Compiler&)            = delete;
		Compiler& operator=(const Compiler&) = delete;
		Compiler(Compiler&&)                 = delete;
		Compiler& operator=(Compiler&&)      = delete;

		/// Full compilation. Also used as the fallback when the set of functions changed.
		CompileStats compile(const fs::File& file);

		/// Incremental recompilation with new source for the same program: diffs the
		/// per-function hashes, rewrites the changed Mods and propagates.
		CompileStats update(const fs::File& file);

	private:
		struct State;
		struct AnalyzedProgram;

		CompileStats    compileAnalyzed(AnalyzedProgram program);
		AnalyzedProgram analyze(const fs::File& file);
		void            throwOnErrors() const;
		void            link() const;

		std::unique_ptr<State> state;
	};

	void compile(const fs::File& file, const CompileOptions& options);

	inline void compile(const fs::File& file, std::string_view output_path = "psacklinged.out") {
		compile(file, CompileOptions{ .output_path = std::string(output_path) });
	}
}
