#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/thread/low_program/utils.hpp>

// @TODOB docs

namespace vm::loader::compiler::detail {

	namespace high = vm::code::instructions;
	using namespace vm::low::instruction_tags;

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

		void add(const code::Instruction instruction);

		template<code::IsInstruction T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes>
		void lower(Args...) = delete;

	private:
		template<IsMicroInstructionTag T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args... args) {
			result.push_back(makeLowInstruction(T::OPCODE, compiler.lowerArgument(ctx, args)...));
#if (BUILD_TYPE_DEV_DEBUG)
			result.back().opcode_id      = std::to_underlying(T::OPCODE);
			result.back().representation = current_high_instruction_representation;
#endif
			next_instruction_index++;
		}

		void addLabel(opargs::Label label) {
			usize lid = compiler.lowerArgument(ctx, label);
			label_id_to_offset.put(lid, next_instruction_index);
		}
	};

	// -----------------------
	template<>
	void MicroBytecodeBuilder::lower<high::Comment>() {
		// emit nothing
	}

	template<>
	void MicroBytecodeBuilder::lower<high::Op_label>(opargs::Label label) {
		addLabel(label);
	}

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
