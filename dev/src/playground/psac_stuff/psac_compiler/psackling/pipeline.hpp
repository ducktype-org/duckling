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

#include <base/types/bit256.hpp>

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

	/// One top-level symbol (constant, function signature, class layout or global
	/// variable) for per-symbol dependency tracking in the incremental backends.
	/// `key` is the symbol's stable identity (kind-prefixed qualified/link name) —
	/// what the PSAC backend keys its per-symbol Mods by; `input_hash` is a SHA-256
	/// over identity AND value — what the query backend uses as the input node.
	struct SymbolInputInfo {
		std::string  key;
		base::Bit256 input_hash;
	};

	/// Stable identity keys of the per-symbol inputs (kind-prefixed, so equal names
	/// of different symbol kinds can never collide).
	std::string constInputKey(const sym::Symbol& symbol);
	std::string functionSigInputKey(const sym::Symbol& symbol);
	std::string globalVarInputKey(const sym::Symbol& symbol);
	std::string classLayoutInputKey(const tp::TypeInfo& type_info);

	/// Input hash of a constant: namespace-qualified name, value bits and type name,
	/// so both value and type changes are detected.
	base::Bit256 constInputHash(const Context& ctx, const sym::Symbol& symbol);

	/// Input hash of a function signature: link name plus parameter and return type
	/// names. Deliberately excludes parameter names and the body, so only changes
	/// callers can observe invalidate them.
	base::Bit256 functionSigInputHash(const Context& ctx, const sym::Symbol& symbol);

	/// Input hash of a global variable: link name and type — what readers depend on.
	/// The initializer is covered by the globals-definitions hash instead (only the
	/// globals object cares about it).
	base::Bit256 globalVarInputHash(const Context& ctx, const sym::Symbol& symbol);

	/// Input hash of a class layout: qualified name plus field names and field type
	/// names in declaration order. Nested layout changes are caught through the
	/// nested classes' own inputs (lowering a struct recursively visits its field
	/// types).
	base::Bit256 classLayoutInputHash(const Context& ctx, const tp::TypeInfo& type_info);

	/// Everything the front end produces for one program: parsed modules in
	/// dependency post-order, populated symbol tables, and the per-function work
	/// items with their content hashes. Shared by the PSAC (psacklinger) and the
	/// query-framework (querylinger) incremental backends.
	struct AnalyzedProgram {
		// Dependency post-order: imported files first, the root file last.
		std::vector<LoadedModule>     modules;
		Context                       ctx;
		std::vector<FunctionWorkItem> items;
		std::vector<base::Bit256>     fn_hashes;
		std::vector<std::string>      link_names;
		std::vector<GlobalInitItem>   global_inits;

		/// Coarse hash over everything a function may depend on besides its body:
		/// classes, constants, globals, imports, namespace structure and function
		/// signatures. The PSAC backend's whole-world dependency.
		base::Bit256 globals_hash;

		/// Only the structural part (imports, namespace names, module identity) —
		/// the query backend's coarse remainder; everything else it tracks through
		/// the per-symbol inputs below.
		base::Bit256 globals_hash_no_consts;

		/// Full content hash of all global variable declarations (initializers
		/// included) in declaration order; the globals-object emission depends on it.
		base::Bit256 global_defs_hash;

		/// Per-symbol inputs of the program (identity + value hashes).
		std::vector<SymbolInputInfo> const_inputs;
		std::vector<SymbolInputInfo> sig_inputs;
		std::vector<SymbolInputInfo> class_inputs;
		std::vector<SymbolInputInfo> global_var_inputs;
	};

	/// Parses the program rooted at `file` (imports loaded transitively) and runs
	/// the global pass and work-item collection over all modules.
	AnalyzedProgram analyzeProgram(const fs::File& file);

	/**
	 * @brief Lowers a single function into its own LLVM module and returns the object
	 * file bytes.
	 *
	 * Safe to run on a worker thread: takes a private copy of the symbol tables (local
	 * declarations mutate them) and a private LLVMContext (LLVM contexts are not
	 * thread-safe). Cross-function calls become declarations resolved by the linker
	 * through the functions' link names.
	 *
	 * Deliberately free of side effects (no filesystem writes) so the query-based
	 * backend can use it as a pure query provider; the caller decides where the
	 * bytes go.
	 *
	 * The observer (if any) is installed on the private Context copy and notified
	 * about symbol uses during lowering.
	 */
	std::string compileOneFunctionToBytes(
		const Context& base_ctx, const FunctionWorkItem& item, SymbolUseObserver* observer = nullptr
	);

	/// compileOneFunctionToBytes + write to `object_path` (the PSAC backend's shape).
	void compileOneFunction(
		const Context&               base_ctx,
		const FunctionWorkItem&      item,
		const std::filesystem::path& object_path,
		SymbolUseObserver*           observer = nullptr
	);

	/**
	 * @brief Builds the object file that defines all global variables (zero-initialized)
	 * and the builtin runtime functions and returns its bytes. Initializer expressions
	 * are lowered in declaration order into a synthetic constructor registered in
	 * llvm.global_ctors. Side-effect free, like compileOneFunctionToBytes.
	 */
	std::string emitGlobalsObjectBytes(
		const Context&                     base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
		SymbolUseObserver*                 observer = nullptr
	);

	/// emitGlobalsObjectBytes + write to `object_path` (the PSAC backend's shape).
	void emitGlobalsObject(
		const Context&                     base_ctx,
		const std::vector<GlobalInitItem>& global_inits,
		const std::filesystem::path&       object_path,
		SymbolUseObserver*                 observer = nullptr
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
