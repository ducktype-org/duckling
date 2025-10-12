#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/thread/low_program/utils.hpp>

namespace vm::loader::compiler {

	namespace high = vm::code::instructions;
	using namespace vm::low::instruction_tags;

	class MicroBytecodeBuilder {
		CRef<Compiler>                             compiler;
		CRef<Compiler::FunctionCompilationContext> ctx;

		low::MicroBytecode result;

		// @TODOB once you're done experimenting with argument lowering,
		// inline this function in the X-macro thingy
		u64 lowerArgument(const opargs::OpCodeArg& arg) {
			//
			(void) arg;
			return 42;
			// return compiler->lowerArgument(*ctx, arg);
		}

		template<IsMicroInstructionTag T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args...);

		template<IsMicroInstructionTag T>
		requires std::same_as<std::tuple<>, typename T::ArgTypes> void addLow() {
			result.push_back({});
		}

		template<IsMicroInstructionTag T, typename Arg0>
		requires std::same_as<std::tuple<Arg0>, typename T::ArgTypes> void addLow(Arg0 arg0) {
			result.push_back({ .arg0 = lowerArgument(arg0) });
		}

		template<IsMicroInstructionTag T, typename Arg0, typename Arg1>
		requires std::same_as<std::tuple<Arg0, Arg1>, typename T::ArgTypes>
		void addLow(Arg0 arg0, Arg1 arg1) {
			result.push_back({ .arg0 = lowerArgument(arg0), .arg1 = lowerArgument(arg1) });
		}

	public:
		template<typename T, typename... Args>
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
