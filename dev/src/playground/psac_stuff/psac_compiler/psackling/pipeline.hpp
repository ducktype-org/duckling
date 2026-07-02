#pragma once

#include "context.hpp"
#include "module_loader.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace pg {
	/// One unit of parallel work: a single function definition together with the
	/// namespace it was declared in (so lowering resolves symbols in the right scope).
	struct FunctionWorkItem {
		pst::Access<pst::Fun> stmt;
		ns::NamespaceID       enclosing_ns;
	};

	/// A global variable with an initializer expression. Lowered (in declaration
	/// order) into the synthetic constructor of the globals object file.
	struct GlobalInitItem {
		pst::Access<pst::Variable> stmt;
		ns::NamespaceID            enclosing_ns;
	};

	/// Everything the front end produces for one program: parsed modules in
	/// dependency post-order, populated symbol tables, and the per-function work
	/// items with their content hashes. Shared by the PSAC (psacklinger) and the
	/// query-framework (querylinger) incremental backends.
	struct AnalyzedProgram {
		// Dependency post-order: imported files first, the root file last.
		std::vector<LoadedModule>     modules;
		Context                       ctx;
		std::vector<FunctionWorkItem> items;
		std::vector<u64>              fn_hashes;
		std::vector<std::string>      link_names;
		std::vector<GlobalInitItem>   global_inits;
		u64                           globals_hash = 0;
	};

	/// Parses the program rooted at `file` (imports loaded transitively) and runs
	/// the global pass and work-item collection over all modules.
	AnalyzedProgram analyzeProgram(const fs::File& file);

	/**
	 * @brief Lowers a single function into its own LLVM module and emits an object file.
	 *
	 * Safe to run on a worker thread: takes a private copy of the symbol tables (local
	 * declarations mutate them) and a private LLVMContext (LLVM contexts are not
	 * thread-safe). Cross-function calls become declarations resolved by the linker
	 * through the functions' link names.
	 */
	void compileOneFunction(
		const Context&               base_ctx,
		const FunctionWorkItem&      item,
		const std::filesystem::path& object_path
	);

	/**
	 * @brief Emits the object file that defines all global variables (zero-initialized)
	 * and the builtin runtime functions. Initializer expressions are lowered in
	 * declaration order into a synthetic constructor registered in llvm.global_ctors.
	 */
	void emitGlobalsObject(
		const Context&                     base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
		const std::filesystem::path&       object_path
	);

	/// Links the object files into an executable with the system clang.
	void linkObjects(
		const std::vector<std::filesystem::path>& object_paths,
		const std::filesystem::path&              globals_object_path,
		const std::string&                        output_path
	);

	/// Wall-clock phase timer, enabled with PSACKLINGER_TIMING=1; prints to stderr.
	class PhaseTimer final {
	public:
		PhaseTimer(const char* phase);
		~PhaseTimer();

		PhaseTimer(const PhaseTimer&)            = delete;
		PhaseTimer& operator=(const PhaseTimer&) = delete;
		PhaseTimer(PhaseTimer&&)                 = delete;
		PhaseTimer& operator=(PhaseTimer&&)      = delete;

		static bool enabled();

	private:
		const char*                           phase;
		std::chrono::steady_clock::time_point start;
	};
}
