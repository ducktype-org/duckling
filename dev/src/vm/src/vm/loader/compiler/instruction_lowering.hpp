#pragma once

#include "compiler.hpp"

#include <base/macros/for_each.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <array>

namespace vm::loader::compiler {

	namespace high = vm::code::instructions;

	class InstructionLowerer {
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

	public:
		template<typename T, typename... Args>
		requires std::same_as<std::tuple<Args...>, typename T::ArgTypes> auto lower(Args...);

		template<>
		auto lower<high::Op_add_l64_imm>(opargs::StackLocal64 var, opargs::Immediate n) {
			return std::array{
				makeLow<add_l64_imm>(var, n),
			};
		}

		template<>
		auto lower<high::Comment>() {
			return std::array<MicroInstruction, 0>{};
		}
	};

	template<typename T>
	constexpr usize LOWERING_SIZE = std::tuple_size_v<decltype(std::apply(
		[](auto... args) { return std::declval<InstructionLowerer>().lower<T>(args...); },
		std::declval<typename T::ArgTypes>()
	))>;

	/*
	 * Some helper concepts to verify at compile time (quickly and showing error close to the
	 * source) when lowering is not (propely) implemented for some high bytecode instruction.
	 *
	 * Makes sure InstructionLowerer::lower works for all alternatives of vm::code::Instruction,
	 * i.e. all template specialisations exists and return a std::array<MicroInstruction, N> for
	 * some N.
	 */
	namespace {
		template<typename T>
		concept IsMicroInstructionsStdArray = requires {
			// required in order to use tuple_size_v safely (clang clashes otherwise :O)
			typename std::tuple_size<T>::type;

			requires std::same_as<T, std::array<MicroInstruction, std::tuple_size_v<T>>>;
		};

		// Checks if InstructionLowerer::lower<T> exists and returns a MicroInstruction array.
		template<typename T>
		concept LoweringWorksForInstr = requires {
			{
				std::apply(
					[](auto... args) {
						return std::declval<InstructionLowerer>().lower<T>(args...);
					},
					std::declval<typename T::ArgTypes>()
				)
			} -> IsMicroInstructionsStdArray;
		};

		// Checks if the above concept holds for alternatives of variant V.
		template<typename V>
		concept LoweringWorksForVariant = []<typename... Alts>(std::variant<Alts...>*) {
			return (LoweringWorksForInstr<Alts> && ...);
		}(static_cast<V*>(nullptr));

		static_assert(
			LoweringWorksForVariant<
				std::variant<high::Op_add_l64_imm, high::Comment>>,  // @TODOB make this
		                                                             // code::Instruction
			"Lowering not implemented for all high bytecode instructions"
			" / some lowering does not return a microinstructions array"
		);
	}
}
