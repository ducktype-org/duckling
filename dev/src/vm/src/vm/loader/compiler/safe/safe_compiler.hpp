#pragma once

#include "../compiler.hpp"

#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/micro_instruction_args.hpp>

namespace vm::loader::compiler::safe {
	namespace detail {
		class MicroBytecodeBuilder;
		template<typename ToType>
		struct LowerArgumentImpl;

		/**
		 * @brief Stores the shared, global state required for the entire compilation process.
		 */
		struct SafeProgramCompilationContext {
			/**
			 * @brief A mapping from a method's string name (`StrID`) to its unique numeric ID.
			 * This is a crucial lookup table used during the instruction lowering phase to
			 * resolve symbolic method names into numeric IDs.
			 */
			base::HashMap<base::StrID, u64> method_name_to_id;

			/**
			 * @brief Count of the global variables compiled up to this point.
			 */
			usize global_count = 0;
			/**
			 * @brief Total size of the globals compiled up to this point, in bytes.
			 */
			Bytes global_buffer_size = Bytes(0);
		};
	}

	class SafeCompiler final: public vm::loader::compiler::Compiler {
		friend class detail::MicroBytecodeBuilder;
		template<typename ToType>
		friend struct detail::LowerArgumentImpl;

	public:
		SafeCompiler(const code::ValidProgram& high_program):
			  vm::loader::compiler::Compiler(high_program) {
			recompile();
		}

		/**
		 * @brief Provides read-only access to the internally managed `LowVMProgram`.
		 * @return A constant reference to the current, fully compiled low-level program.
		 */
		CRef<vm::low::LowVMProgram> getLowProgram() const;

	protected:
		ProgramSize getCurrentProgramSize() const override;

		void compileNewTypes(const std::vector<code::valid_type::ValidType>& new_types) override;
		void compileNewGlobals(const std::vector<code::GlobalData>& new_globals) override;
		void compileNewFunctions(const std::vector<code::Function>& new_functions) override;
		void compileNewExtCFunctions(const std::vector<code::ExternalCFunction>& new_functions
		) override;

	private:
		/**
		 * @brief The microbytecode program representation being built and managed by the compiler.
		 */
		vm::low::LowVMProgram low_program;

		detail::SafeProgramCompilationContext program_ctx;

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
		low::MicroBytecode lowerInstructions(
			const vm::loader::compiler::detail::FunctionStackContext& ctx
		);

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
		u64 lowerArgument(
			const vm::loader::compiler::detail::FunctionStackContext& local_ctx,
			base::HashMap<base::StrID, usize>&                        label_id_map,
			const FromType&                                           opcode_arg
		);
	};
}
