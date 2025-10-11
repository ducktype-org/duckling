#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <array>

namespace vm::loader::compiler {

	namespace high = vm::code::instructions;

	// @TODOB move to the place where the concepts are defined and the like
	namespace {
		template<typename T>
		struct Meta;

#define HANDLE_INSTR_0ARGS(instr)            \
	template<>                               \
	struct Meta<VM_INSTR_FROM_NAME(instr)> { \
		using Args = std::tuple<>;           \
	};
#define HANDLE_INSTR_1ARGS(instr, arg0)      \
	template<>                               \
	struct Meta<VM_INSTR_FROM_NAME(instr)> { \
		using Args = std::tuple<arg0>;       \
	};
#define HANDLE_INSTR_2ARGS(instr, arg0, arg1) \
	template<>                                \
	struct Meta<VM_INSTR_FROM_NAME(instr)> {  \
		using Args = std::tuple<arg0, arg1>;  \
	};
#include <vm/bytecode/instruction_definitions.hpp>
#undef HANDLE_INSTR_0ARGS
#undef HANDLE_INSTR_1ARGS
#undef HANDLE_INSTR_2ARGS
	}

	struct InstructionLowerer {
		CRef<Compiler>                             compiler;
		CRef<Compiler::FunctionCompilationContext> ctx;
		usize                                      instruction_index;

		u64 lowerArgument(const opargs::OpCodeArg& arg) {
			return compiler->lowerArgument(*ctx, instruction_index, arg);
		}

		template<typename T, typename... Args>
		MicroInstruction makeLow(Args...);

#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	struct INSTR {};                    \
	template<>                          \
	MicroInstruction makeLow<INSTR>() { \
		return {};                      \
	}
#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)    \
	struct INSTR {};                             \
	template<>                                   \
	MicroInstruction makeLow<INSTR>(ARG0 arg0) { \
		return { .arg0 = lowerArgument(arg0) };  \
	}
#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                          \
	struct INSTR {};                                                         \
	template<>                                                               \
	MicroInstruction makeLow<INSTR>(ARG0 arg0, ARG1 arg1) {                  \
		return { .arg0 = lowerArgument(arg0), .arg1 = lowerArgument(arg1) }; \
	}

#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

		template<typename T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename Meta<T>::Args> auto lower(Args...);

		template<>
		auto lower<high::Op_add_l64_imm>(opargs::StackLocal64 var, opargs::Immediate n) {
			return std::array{
				makeLow<add_l64_imm>(var, n),
			};
		}
	};
}
