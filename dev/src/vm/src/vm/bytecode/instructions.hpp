/**
 * @brief This file contains structures representing VM instructions.
 * Each opcode has a corresponding structure with name `Op_{opcode_name}`.
 * A variant structure that can store any instruction is called `Instruction`.
 */

#pragma once

#include "element_base.hpp"

#include <base/box.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <variant>

#define VM_INSTR_FROM_NAME(opcode) vm::code::instructions::Op_##opcode

namespace vm::code {
	/**
	 * @brief `instructions` namespace encapsulates available VM instructions.
	 */
	namespace instructions {
#define HANDLE_INSTR_0ARGS(opcode)                                                    \
	struct Op_##opcode final: ElementBase {                                           \
		using ArgTypes = std::tuple<>;                                                \
		constexpr bool operator==(const Op_##opcode&) const noexcept { return true; } \
	};

#define HANDLE_INSTR_1ARGS(opcode, arg0_type)                                \
	struct Op_##opcode final: ElementBase {                                  \
		Op_##opcode(arg0_type arg0): arg0(arg0) {}                           \
		using ArgTypes = std::tuple<arg0_type>;                              \
		arg0_type      arg0;                                                 \
		constexpr bool operator==(const Op_##opcode& other) const noexcept { \
			return arg0 == other.arg0;                                       \
		}                                                                    \
	};

#define HANDLE_INSTR_2ARGS(opcode, arg0_type, arg1_type)                       \
	struct Op_##opcode final: ElementBase {                                    \
		Op_##opcode(arg0_type arg0, arg1_type arg1): arg0(arg0), arg1(arg1) {} \
		using ArgTypes = std::tuple<arg0_type, arg1_type>;                     \
		arg0_type      arg0;                                                   \
		arg1_type      arg1;                                                   \
		constexpr bool operator==(const Op_##opcode& other) const noexcept {   \
			return arg0 == other.arg0 && arg1 == other.arg1;                   \
		}                                                                      \
	};

#include <vm/bytecode/instruction_definitions.hpp>

#undef HANDLE_INSTR_0ARGS
#undef HANDLE_INSTR_1ARGS
#undef HANDLE_INSTR_2ARGS

		/**
		 * @brief An extra instruction that represents a comment.
		 * @note It also helps with macro, because without it the template
		 * below would finish with a `,`, which does not compile.
		 */
		struct Comment final: ElementBase {
			Comment()      = default;
			using ArgTypes = std::tuple<>;

			Comment(base::StrID comment): comment(comment) {}

			base::StrID comment;

			constexpr bool operator==(const Comment& other) const noexcept {
				return comment == other.comment;
			}
		};
	}

	using Instruction = std::variant<
#define HANDLE_INSTR(opcode) VM_INSTR_FROM_NAME(opcode),
#include <vm/bytecode/instruction_definitions.hpp>
#undef HANDLE_INSTR
		instructions::Comment>;

	template<typename T>
	concept IsInstruction = base::IS_VARIANT_MEMBER_V<std::remove_cvref_t<T>, Instruction>;

	template<typename T>
	concept TwoArgumentOpcode = IsInstruction<T> && std::tuple_size_v<typename T::ArgTypes> == 2;

	template<typename T>
	concept OneArgumentOpcode = IsInstruction<T> && std::tuple_size_v<typename T::ArgTypes> == 1;

	template<typename T>
	concept ZeroArgumentOpcode = IsInstruction<T> && std::tuple_size_v<typename T::ArgTypes> == 0;

}
