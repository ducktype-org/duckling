#pragma once

#include <filesystem/file.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace ql {
	inline constexpr std::string_view QUERYLINGER_VERSION = "Querylinger version -0.05";

	struct CompileOptions {
		std::string output_path = "querylinged.out";

		/// Number of query-framework worker threads; 0 keeps the current worker count
		/// (set it up front via concurrent::worker::setWorkerCount).
		usize num_threads = 0;
	};

	struct CompileStats {
		usize total_functions      = 0;
		usize recompiled_functions = 0;
	};

	/**
	 * @brief Compiler session that keeps the query graph alive between compiles, so
	 * feeding it new source recompiles only the functions whose content (or the
	 * global declarations they depend on) actually changed.
	 *
	 * The psacklinger counterpart drives PSAC Mods and psac_propagate; here every
	 * function's object emission is a query depending on (function body, globals)
	 * side inputs, and query::external::invalidateQueries plays psac_propagate's
	 * role: the session diffs the input sets between compiles and re-schedules all
	 * emission queries, of which only the invalidated ones actually run.
	 */
	class Compiler final {
	public:
		/// Private session state; defined in session.cpp only.
		struct State;

		explicit Compiler(CompileOptions options);
		~Compiler();

		Compiler(const Compiler&)            = delete;
		Compiler& operator=(const Compiler&) = delete;
		Compiler(Compiler&&)                 = delete;
		Compiler& operator=(Compiler&&)      = delete;

		/// Full compilation of the program rooted at `file` (imported files are loaded
		/// transitively).
		CompileStats compile(const fs::File& file);

		/// Incremental recompilation with new source for the same program: re-analyzes
		/// the program, diffs the side-input sets, invalidates exactly the dependent
		/// queries and re-runs the emission.
		CompileStats update(const fs::File& file);

	private:
		CompileStats runBackendAndLink(bool allow_skip_link);

		std::unique_ptr<State> state;
	};

	void compile(const fs::File& file, const CompileOptions& options);

	inline void compile(const fs::File& file, std::string_view output_path = "querylinged.out") {
		compile(file, CompileOptions{ .output_path = std::string(output_path) });
	}
}
