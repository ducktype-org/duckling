#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/thread/low_program/utils.hpp>

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
		std::string currentHighInstructionRepresentation{};
#endif

	public:
		MicroBytecodeBuilder(Compiler& compiler, Compiler::FunctionCompilationContext& ctx):
			  compiler{ compiler },
			  ctx{ ctx } {}

		std::pair<low::MicroBytecode, decltype(label_id_to_offset)> build() {
			return { std::move(result), std::move(label_id_to_offset) };
		}

		void add(const code::Instruction instruction) {
#if (BUILD_TYPE_DEV_DEBUG)
			currentHighInstructionRepresentation = code::instructionToString(instruction);
#endif
			// clang-format off
            VARIANT_VISIT(instruction, 
                VISIT_CASE(code::ZeroArgumentOpcode auto, i, lower<decltype(i)>();)
                VISIT_CASE(code::OneArgumentOpcode auto, i, lower<decltype(i)>(i.arg0);)
                VISIT_CASE(code::TwoArgumentOpcode auto, i, lower<decltype(i)>(i.arg0, i.arg1);)
            );
			// clang-format on
		}

		template<code::IsInstruction T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes>
		void lower(Args...) = delete;

	private:
		void fillOutDebugData() {
#if (BUILD_TYPE_DEV_DEBUG)
			auto& instruction          = result.back();
			instruction.representation = currentHighInstructionRepresentation;
			instruction.opcode_id      = getInstructionOpcode(instruction);
#endif
		}

		// @TODOB is this needed? the below are not full template specialisations
		template<IsMicroInstructionTag T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args...);

		template<IsMicroInstructionTag T>
		requires std::same_as<std::tuple<>, typename T::ArgTypes> void addLow() {
			result.push_back(makeLowInstruction(T::OPCODE));
			fillOutDebugData();
			next_instruction_index++;
		}

		template<IsMicroInstructionTag T, typename Arg0>
		requires std::same_as<std::tuple<Arg0>, typename T::ArgTypes> void addLow(Arg0 arg0) {
			result.push_back(makeLowInstruction(T::OPCODE, compiler.lowerArgument(ctx, arg0)));
			fillOutDebugData();
			next_instruction_index++;
		}

		template<IsMicroInstructionTag T, typename Arg0, typename Arg1>
		requires std::same_as<std::tuple<Arg0, Arg1>, typename T::ArgTypes>
		void addLow(Arg0 arg0, Arg1 arg1) {
			result.push_back(makeLowInstruction(
				T::OPCODE, compiler.lowerArgument(ctx, arg0), compiler.lowerArgument(ctx, arg1)
			));
			fillOutDebugData();
			next_instruction_index++;
		}

		void addLabel(opargs::Label label) {
			usize lid = compiler.lowerArgument(ctx, label);
			label_id_to_offset.put(lid, next_instruction_index);
		}

		// -----------------------

		template<>
		void lower<high::Op_add_l64_imm>(opargs::StackLocal64 var, opargs::Immediate n) {
			addLow<Op_add_l64_imm>(var, n);
		}

		template<>
		void lower<high::Comment>() {}

		// -----------------------
	};
}
