#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/micro_instruction_args.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::loader::compiler {
	namespace detail {
		class MicroBytecodeBuilder;
		template<typename ToType>
		struct LowerArgumentImpl;
	}

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
		friend class detail::MicroBytecodeBuilder;
		template<typename ToType>
		friend struct detail::LowerArgumentImpl;

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
		 *
		 * @warning This function is stateful and operates incrementally. It is crucial
		 * that each `high_program` passed to this function is an extension of the one from the
		 * previous call. The compiler assumes that the existing set of program elements (types,
		 * functions, etc.) is a stable prefix of the new set. Passing a completely unrelated
		 * `ValidProgram` will lead to an invalid internal state and incorrect compilation.
		 */
		void recompile(const code::ValidProgram& high_program);

		/**
		 * @brief Provides read-only access to the internally managed `LowVMProgram`.
		 * @return A constant reference to the current, fully compiled low-level program.
		 */
		CRef<vm::low::LowVMProgram> getLowProgram() const;

	private:
		/**
		 * @brief Stores the shared, global state required for the entire compilation process.
		 */
		struct ProgramCompilationContext {
			/**
			 * @brief A mapping from a method's string name (`StrID`) to its unique numeric ID.
			 * This is a crucial lookup table used during the instruction lowering phase to
			 * resolve symbolic method names into numeric IDs.
			 */
			base::HashMap<base::StrID, u64> method_name_to_id;
			/**
			 * @brief A complete list of all bytecode functions which will be added in the
			 * compilation process. Used when lowering call instructions to translate the function
			 * name to it's index.
			 */
			ObjIdNameMap<code::Function> function_forward_declarations;

			/**
			 * @brief All available ExternCFunctions callable from the program.
			 * Used when lowering call_cfunc instructions to translate the function name to it's
			 * index.
			 */
			ObjIdNameMap<code::ExternalCFunction> ext_c_functions;

			/**
			 * @brief Count of the global variables compiled up to this point.
			 */
			usize global_count = 0;
			/**
			 * @brief Total size of the globals compiled up to this point, in bytes.
			 */
			usize global_buffer_size = 0;
		};

		/**
		 * @brief A structure holding the intermediate state for the compilation of a single function.
		 */
		struct FunctionCompilationContext {
			FunctionCompilationContext(const code::Function& func): function(func) {}

			/// The high level function definition.
			const code::Function& function;
			/// Temporary label IDs used before label linking.
			base::HashMap<base::StrID, usize> label_id_map;

			struct LocalEntry {
				u64      offset;
				u64      block_idx;
				TypeCRef type;
			};

			/// A mapping from a local variable's name to its offset on the function's local stack
			/// and type.
			base::HashMap<base::StrID, LocalEntry> locals_map{};
			/// Total required size for the local stack frame, in bytes.
			usize local_stack_size  = 0;
			usize local_block_count = 0;
		};

		/**
		 * @brief The microbytecode program representation being built and managed by the compiler.
		 */
		vm::low::LowVMProgram     low_program;
		ProgramCompilationContext program_ctx;

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
		 * @brief Compiles newly added ExternCFunctions and adds the compiled functions to the
		 * internal `low_program.extern_c_functions`.
		 */
		void compileNewExtCFunctions(const std::vector<code::ExternalCFunction>& new_functions);

		/**
		 * @brief Calculates the stack offsets of stack variables.
		 * Since in ValidProgram variables are represented by names not indexes on the stack.
		 * This function creates an offset map which is used in `lowerInstructions` to change the
		 * variable names to numeric offsets.
		 * @note Assumes all variables in the program have a unique name.
		 */
		void calculateOffsets(FunctionCompilationContext& ctx);

		/**
		 * @brief Fills out label arguments from IDs to label offsets in micro-bytecode.
		 * Since a single high bytecode instruction can lower into many micro instructions,
		 * we do not know in advance where labels land after lowering.
		 * Instead `MicroBytecodeBuilder` generates temporary label IDs and calculates label
		 * offsets during building. This function uses this information to go through
		 * the instructions again and fill out the correct offsets.
		 */
		void linkLabelArguments(
			low::MicroBytecode& instructions, const base::HashMap<usize, usize>& label_map
		);

		/**
		 * @brief Lowers instructions to micro-bytecode. Iterates through the label-less
		 * instructions and translates them into a sequence of `MicroInstruction`, resolving all
		 * symbolic arguments to numeric values.
		 * @return The converted list of instructions.
		 */
		low::MicroBytecode lowerInstructions(FunctionCompilationContext& ctx);

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
		template<opargs::ArgumentType FromType, low::opargs::ArgumentType ToType>
		u64 lowerArgument(FunctionCompilationContext& local_ctx, const FromType& opcode_arg);
	};

}
