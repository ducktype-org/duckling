#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/thread/low_program/utils.hpp>

namespace vm::loader::compiler::detail {

	namespace high = vm::code::instructions;
	using namespace vm::low::instruction_tags;

	/**
	 * Helper class for lowering high bytecode instructions.
	 * This class simply holds all the relevant context and defines some helper methods, which make
	 * defining instruction lowering recipes free from extra context arguments noise, typesafe and
	 * macro-free. When adding a new instruction simply provide a new `lower` specialisation like so:
	 * ```
	 * template<>
	 * void MicroBytecodeBuilder::lower<high::Op_do_something_complex>(
	 *     opargs::Foo foo, opargs::Bar bar
	 * ) {
	 *     addLow<Op_first_step>(foo, bar);
	 *     addLow<Op_second_step>(foo);
	 *     addLow<Op_finish_up_the_thing>();
	 * }
	 * ```
	 * You will get a compile time error (sadly a big one) if you forget to implement lowering for
	 * an instruction. Micro instruction arguments are type-checked.
	 *
	 * Beside generating a vector of `MicroInstruction`s, this class also provides a map
	 * from temporary label IDs to label offsets used later by `Compiler::linkLabelArguments`.
	 */
	class MicroBytecodeBuilder {
		Compiler&                             compiler;
		Compiler::FunctionCompilationContext& ctx;

		base::HashMap<usize, usize> label_id_to_offset{};
		usize                       next_instruction_index = 0;

		low::MicroBytecode result;

#if (BUILD_TYPE_DEV_DEBUG)
		std::string current_high_instruction_representation{};
#endif

	public:
		MicroBytecodeBuilder(Compiler& compiler, Compiler::FunctionCompilationContext& ctx):
			  compiler{ compiler },
			  ctx{ ctx } {}

		std::pair<low::MicroBytecode, decltype(label_id_to_offset)> build() {
			return { std::move(result), std::move(label_id_to_offset) };
		}

		/// Add a new high instruction.
		void add(const code::Instruction instruction);


	private:
		// Must be specialized per high-level instruction. Intentionally `=delete`d so a missing
		// specialization produces a clear compile-time error (early, in editor, not at linking).
		// Keep NOLINT because clang-tidy likes to have all `=delete` public. The rule is made for
		// enforcing `Foo() = delete` over private constructors, but here the specialisations
		// get "un-deleted" and this method is not meant to be called by the outside world.
		template<code::IsInstruction T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes>
		void lower(Args...) = delete;  // NOLINT(modernize-use-equals-delete)

		template<IsMicroInstructionTag T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args... args) {
			result.push_back(makeLowInstruction(T::OPCODE, compiler.lowerArgument(ctx, args)...));
#if (BUILD_TYPE_DEV_DEBUG)
			result.back().opcode_id      = T::OPCODE;
			result.back().representation = current_high_instruction_representation;
#endif
			next_instruction_index++;
		}

		void addLabel(opargs::Label label) {
			usize lid = compiler.lowerArgument(ctx, label);
			label_id_to_offset.put(lid, next_instruction_index);
		}
	};

	// Lowering recipes:
	// -----------------

	template<>
	void MicroBytecodeBuilder::lower<high::Comment>() {
		// emit nothing
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_label>(opargs::Label label) {
		addLabel(label);
	}

// @TODO #1189: Define proper lowering recipes once the translation gets nontrivial.
#define HANDLE_MICRO_INSTR_0ARGS(INSTR)                    \
	template<>                                             \
	void MicroBytecodeBuilder::lower<high::Op_##INSTR>() { \
		addLow<Op_##INSTR>();                              \
	}
#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)                       \
	template<>                                                      \
	void MicroBytecodeBuilder::lower<high::Op_##INSTR>(ARG0 arg0) { \
		addLow<Op_##INSTR>(arg0);                                   \
	}
#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                            \
	template<>                                                                 \
	void MicroBytecodeBuilder::lower<high::Op_##INSTR>(ARG0 arg0, ARG1 arg1) { \
		addLow<Op_##INSTR>(arg0, arg1);                                        \
	}
#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

	// end of lowering recipes
	// -----------------------

	void MicroBytecodeBuilder::add(const code::Instruction instruction) {
#if (BUILD_TYPE_DEV_DEBUG)
		current_high_instruction_representation = code::instructionToString(instruction);
#endif
		// clang-format off
            VARIANT_VISIT(instruction, 
                VISIT_CASE(code::ZeroArgumentOpcode auto, i, lower<decltype(i)>();)
                VISIT_CASE(code::OneArgumentOpcode auto, i, lower<decltype(i)>(i.arg0);)
                VISIT_CASE(code::TwoArgumentOpcode auto, i, lower<decltype(i)>(i.arg0, i.arg1);)
            );
		// clang-format on
	}
}
