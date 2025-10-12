#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>
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

		// @TODOB is this needed? the below are not full template specialisations
		template<IsMicroInstructionTag T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args...);

		template<IsMicroInstructionTag T>
		requires std::same_as<std::tuple<>, typename T::ArgTypes> void addLow() {
			result.push_back(makeLowInstruction(T::OPCODE));
			next_instruction_index++;
		}

		template<IsMicroInstructionTag T, typename Arg0>
		requires std::same_as<std::tuple<Arg0>, typename T::ArgTypes> void addLow(Arg0 arg0) {
			result.push_back(makeLowInstruction(T::OPCODE, compiler.lowerArgument(ctx, arg0)));
			next_instruction_index++;
		}

		template<IsMicroInstructionTag T, typename Arg0, typename Arg1>
		requires std::same_as<std::tuple<Arg0, Arg1>, typename T::ArgTypes>
		void addLow(Arg0 arg0, Arg1 arg1) {
			result.push_back(makeLowInstruction(
				T::OPCODE, compiler.lowerArgument(ctx, arg0), compiler.lowerArgument(ctx, arg1)
			));
			next_instruction_index++;
		}

		void addLabel(opargs::Label label) {
			usize lid = compiler.lowerArgument(ctx, label);
			label_id_to_offset.put(lid, next_instruction_index);
		}

	public:
		MicroBytecodeBuilder(Compiler& compiler, Compiler::FunctionCompilationContext& ctx):
			  compiler{ compiler },
			  ctx{ ctx } {}

		std::pair<low::MicroBytecode, decltype(label_id_to_offset)> build() {
			return { std::move(result), std::move(label_id_to_offset) };
		}

		template<code::IsInstruction T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void lower(Args...);

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

// @TODOB add back the macro checking if everything is defined
