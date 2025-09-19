#pragma once
#include "base/maps.hpp"
#include "base/optional.hpp"
#include "base/string_id.hpp"

#include "vm/bytecode/bytecode.hpp"
#include "vm/bytecode/opcode_args.hpp"
#include "vm/bytecode/type_of_data.hpp"
#include "vm/bytecode/validator/type_context.hpp"
#include "vm/bytecode/validator/valid_program.hpp"
#include "vm/core/process/type_metadata/definitions.hpp"
#include "vm/core/process/type_metadata/type.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include "vm/core/thread/low_program/low_program.hpp"
#include "vm/utils/stable_obj_id_name_map.hpp"
#include <vm/loader/loader.hpp>

namespace vm::loader::compiler {
	/**
	 * @class Compiler
	 * @brief A stateful, incremental bytecode compiler.
	 *
	 * This class acts as a builder for a `vm::low::LowVMProgram`. It maintains an internal,
	 * low-level representation of the program and updates it incrementally when new high-level
	 * code is provided. This stateful approach avoids recompiling the entire program on each
	 * code injection, compiling only the new elements (types, globals, and functions).
	 */
	class Compiler {
	public:
		Compiler() = default;

		/**
		 * @brief Incrementally recompiles and updates the internal `LowVMProgram`.
		 * This is the main entry point for the compiler. It compares the provided `high_program`
		 * with its internal state to identify new types, globals, and functions. It then
		 * compiles only the new elements and adds them to the internal `LowVMProgram`.
		 *
		 * @param high_program The new, complete, and validated high-level program representation.
		 *
		 * @note The compilation never fails as the given code was statically verified.
		 */
		void recompile(const code::HighVMProgram& high_program);

		/**
		 * @brief Provides read-only access to the internally managed `LowVMProgram`.
		 * @return A constant reference to the current, fully compiled low-level program.
		 */
		const low::LowVMProgram& getLowProgram() const;

	private:
		/**
		 * @brief The microbytecode program representation being built and managed by the compiler.
		 */
		low::LowVMProgram low_program;

		/**
		 * @brief A mapping from a method's string name (`StrID`) to its unique numeric ID.
		 * This is a crucial lookup table used during the instruction lowering phase to
		 * resolve symbolic method names into numeric IDs.
		 */
		base::HashMap<base::StrID, u64> method_name_to_id;

		/**
		 * @brief A structure holding the intermediate state for the compilation of a single function.
		 */
		struct FunctionCompilationContext {
			/// Function after the label instructions have been removed.
			base::Optional<code::Function> function{};
			/// A mapping from a label's name to it's instruction index in the function instruction list.
			base::HashMap<base::StrID, usize> label_positions{};
			/// A mapping from a local variable's name to its offset on the function's local stack.
			base::HashMap<base::StrID, usize> local_offset_map{};
			/// Total required size for the local stack frame, in bytes.
			usize local_stack_size = 0;
		};

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
		void compileNewTypes(const code::TypeContext& ctx);

		/**
		 * @brief Compiles newly added global variables.
		 * Translates `code::GlobalData` objects into `low::LowGlobalData` and adds them
		 * to the internal `low_program.global_data` collection.
		 * @param new_globals A vector containing the new globals to add.
		 */
		void compileNewGlobals(const std::vector<code::GlobalData>& new_globals);

		/**
		 * @brief Compiles newly added functions.
		 * For each new function, it performs the full compilation pipeline and appends the
		 * resulting `low::LowFuncData` to the internal `low_program.functions` collection.
		 * @param new_functions A vector containing the new `Function` objects for newly added
		 * functions.
		 */
		void compileNewFunctions(const std::vector<code::Function>& new_functions);

		/**
		 * @brief Removes label instructions from the compiled functions code. Calculates label
		 * positions.
		 */
		void splitCodeAndLabels(FunctionCompilationContext& ctx);

		/**
		 * @brief Calculates stack offsets of local variables and the maximum size of the local
		 * stack used by the function.
		 */
		void calculateOffsets(FunctionCompilationContext& ctx);

		/**
		 * @brief Lowers instructions to micro-bytecode. Iterates through the label-less
		 * instructions and translates them into a sequence of `MicroInstruction`, resolving all
		 * symbolic arguments to numeric values.
		 * @return The converted list of instructions.
		 */
		low::MicroByteCode lowerInstructions(const FunctionCompilationContext& ctx);

		/**
		 * @brief Translates a single high-level instruction argument (`opargs::OpCodeArg`)
		 * into its raw 64-bit integer representation used by `MicroInstruction`
		 * This function resolves symbolic names (locals, globals, functions, methods, labels)
		 * into their corresponding numeric offsets, IDs, or relative jumps.
		 * @param local_ctx The local context for the current function.
		 * @param instruction_index The index of the current instruction, needed for relative jump
		 * calculation.
		 * @param opcode_arg The symbolic argument to translate.
		 * @return The 64-bit numeric value of the argument.
		 */
		u64 lowerArgument(
			const FunctionCompilationContext& local_ctx,
			usize                             instruction_index,
			const opargs::OpCodeArg&          opcode_arg
		);
	};

}
