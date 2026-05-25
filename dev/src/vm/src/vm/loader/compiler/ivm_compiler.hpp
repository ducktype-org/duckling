#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::loader::compiler {
	namespace detail {
		/**
		 * @brief A structure holding the intermediate state for the compilation of a single function.
		 */
		struct FunctionStackContext {
			FunctionStackContext(const code::Function& func): function(func) {}

			/// The high level function definition.
			const code::Function& function;

			struct LocalEntry {
				code::valid_type::TypeSize offset;
				u64 stack_index;  /// The index of the local variable in the function's local
				                  /// stack, always equal to the size of the type stack at the
				                  /// moment of the variable declaration.
				code::valid_type::ValidTypeID type;
			};

			/// A mapping from a local variable's name to its offset on the function's local stack
			/// and type.
			base::HashMap<base::StrID, LocalEntry> locals_map{};
			/// Total required size for the local stack frame, in bytes.
			code::valid_type::TypeSize local_stack_size{};
			usize                      local_block_count = 0;
		};
	}

	/**
	 * @brief A structure holding the size information for the program.
	 * Used to determine the size of various internal data structures for incremental compilation.
	 */
	struct ProgramSize {
		usize function_count;
		usize global_count;
		usize type_count;
		usize ext_c_function_count;
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
		 * @brief Incrementally recompiles and updates the internal low-level program.
		 * This is the main entry point for the compiler. It compares the provided `high_program`
		 * with its internal state to identify new types, globals, and functions. It then
		 * compiles only the new elements and adds them to the internal low-level program.
		 *
		 * @param high_program The new, complete, and validated high-level program representation.
		 *
		 * @note The compilation never fails as the given code was statically verified.
		 *
		 * @warning This function is stateful and operates incrementally. It is crucial
		 * that `high_program` passed to this function is an extension of the one from the
		 * previous call. The compiler assumes that the existing set of program elements (types,
		 * functions, etc.) is a stable prefix of the new set. Passing a completely unrelated
		 * `ValidProgram` will lead to an invalid internal state and incorrect compilation.
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
		virtual void compileNewFunctions(const std::vector<code::Function>& new_functions) = 0;

		/**
		 * @brief Compiles newly added ExternCFunctions and adds the compiled functions to the
		 * internal `low_program.extern_c_functions`.
		 */
		virtual void compileNewExtCFunctions(const std::vector<code::ExternalCFunction>& new_functions
		) = 0;

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
		 * This function creates an offset map which is used during lowering instructions to change
		 * the variable names to numeric offsets.
		 */
		detail::FunctionStackContext calculateStackContext(const code::Function& function);
	};
}
