#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/local_stack_database.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::loader::compiler {
	namespace detail {
		/**
		 * @brief A structure holding the intermediate state for the compilation of a single function.
		 * @TODO: #2898 This structure's name is outdated
		 */
		struct FunctionStackContext final {
			FunctionStackContext(const code::valid_function::ValidFunction& func): function(func) {}

			/// The high level function definition.
			const code::valid_function::ValidFunction& function;

			/// Total required size for the local stack frame, in bytes.
			code::valid_type::TypeSize local_stack_size{};
			usize                      local_block_count = 0;

			base::Optional<CRef<SafeVMThread>> thread_evaluating_expr = std::nullopt;
		};
	}

	/**
	 * @brief A structure holding the size information for the program.
	 * Used to determine the size of various internal data structures for incremental compilation.
	 */
	struct ProgramSize final {
		usize function_count       = 0;
		usize global_count         = 0;
		usize type_count           = 0;
		usize ext_c_function_count = 0;
		usize ffi_function_count   = 0;
	};

	/**
	 * @class Compiler
	 * @brief A stateful, incremental bytecode compiler.
	 *
	 * This class acts as a base-class for builders of low-level programs. Does not maintain an
	 * internal state, however subclasses are very welcome to do so. The main point of this class is
	 * to provide the `recompile` method, which performs an incremental compilation of the program.
	 * This stateful approach avoids recompiling the entire program on each code
	 * injection, compiling only the new elements (types, globals, and functions).
	 */
	class IVMCompiler {
	public:
		/**
		 * @brief Constructs a new Compiler that will source its information about the program from
		 * `high_program`.
		 */
		IVMCompiler(const code::ValidProgram& high_program);

		virtual ~IVMCompiler() = default;

		/**
		 * @brief Incrementally re-compiles and updates the internal low-level program.
		 * This is the main entry point for the compiler. It compares the size of `high_program`
		 * with sizes from `getCurrentProgramSize()` to identify new types, globals, and functions.
		 * It then compiles only the new elements.
		 */
		void recompile();

	protected:
		const code::ValidProgram& high_program;

		/**
		 * @brief Processes newly added types and adds them to the existing type_metadata.
		 * It also scans the new types for any new virtual methods and updates the compiler's
		 * internal method-to-ID mappings.
		 *
		 * @note The internal type metadata is not being rebuilt completely. New types are added to
		 * the existing set of types and only them are being rebuilt, thus the old references in
		 * type metadata stay untouched.
		 * @param new_types A vector containing the new types to add.
		 */
		virtual void compileNewTypes(const std::vector<code::valid_type::ValidType>& new_types) = 0;

		/**
		 * @brief Compiles newly added global variables.
		 * Translates `code::GlobalData` objects into `low::LowGlobalData` and adds them
		 * to the internal `low_program.global_data` collection.
		 * @param new_globals A vector containing the new globals to add.
		 */
		virtual void compileNewGlobals(const std::vector<code::GlobalData>& new_globals) = 0;

		/**
		 * @brief Compiles newly added functions.
		 * For each new function, it performs the full compilation pipeline and appends the
		 * resulting `low::LowFuncData` to the internal `low_program.functions` collection.
		 * @param new_functions A vector containing the new `Function` objects for newly added
		 * functions.
		 */
		virtual void compileNewFunctions(
			const std::vector<CRef<code::valid_function::ValidFunction>>& new_functions
		) = 0;

		/**
		 * @brief Compiles newly added ExternCFunctions and adds the compiled functions to the
		 * internal `low_program.extern_c_functions`.
		 */
		virtual void compileNewExtCFunctions(const std::vector<code::ExternalCFunction>& new_functions
		) = 0;

		/**
		 * @brief Compiles newly added FFIFunctions. Compilers without FFI support may keep the
		 * default no-op implementation.
		 */
		virtual void compileNewFFIFunctions(
			[[maybe_unused]] const std::vector<code::FFIFunction>& new_functions
		) {}

		/**
		 * @brief Retrieves the current size of the compiled program, in terms of its various
		 * components (functions, types, globals, etc.). This information is crucial for the
		 * incremental compilation process, as it allows the compiler to identify which elements of
		 * the `high_program` are new and need to be compiled.
		 * @return A `ProgramSize` struct containing the counts of various program components.
		 */
		[[nodiscard]] virtual ProgramSize getCurrentProgramSize() const = 0;

		/**
		 * @brief Calculates the stack offsets of stack variables.
		 * Since in ValidProgram variables are represented by names not indexes on the stack.
		 * This function creates an stack context which is used during lowering instructions to
		 * translate the variable name to numeric offsets.
		 */
		[[nodiscard]] detail::FunctionStackContext calculateStackContext(
			const code::valid_function::ValidFunction& function
		) const;
	};
}
