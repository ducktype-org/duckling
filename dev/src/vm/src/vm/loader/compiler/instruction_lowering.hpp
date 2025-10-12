#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>

namespace vm::loader::compiler {

	namespace high = vm::code::instructions;

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

#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	struct INSTR {                      \
		using ArgTypes = std::tuple<>;  \
	};
#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0) \
	struct INSTR {                            \
		using ArgTypes = std::tuple<ARG0>;    \
	};
#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1) \
	struct INSTR {                                  \
		using ArgTypes = std::tuple<ARG0, ARG1>;    \
	};

#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

		template<typename T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void addLow(Args...);

#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	template<>                          \
	void addLow<INSTR>() {              \
		result.emplace_back();          \
	}
#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)              \
	template<>                                             \
	void addLow<INSTR>(ARG0 arg0) {                        \
		result.push_back({ .arg0 = lowerArgument(arg0) }); \
	}
#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                                     \
	template<>                                                                          \
	void addLow<INSTR>(ARG0 arg0, ARG1 arg1) {                                          \
		result.push_back({ .arg0 = lowerArgument(arg0), .arg1 = lowerArgument(arg1) }); \
	}

#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

	public:
		template<typename T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> void lower(Args...);

		// -----------------------
		template<>
		void lower<high::Op_add_l64_imm>(opargs::StackLocal64 var, opargs::Immediate n) {
			addLow<add_l64_imm>(var, n);
		}

		template<>
		void lower<high::Comment>() {}

		// -----------------------
	};
}
