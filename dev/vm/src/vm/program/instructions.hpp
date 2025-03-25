/**
 * @brief This file contains a list of structures representing VM instructions.
 */

#pragma once

#include <variant>
#include <vm/program/opcode_args.hpp>
#include <base/box.hpp>
#include "element_base.hpp"

#define VM_INSTR_FROM_NAME(opcode) vm::program::instructions::Op_##opcode

namespace vm::program {
	namespace instructions {
#define HANDLE_OPCODE_0ARGS(opcode) \
	struct Op_##opcode: ElementBase {};
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)     \
	struct Op_##opcode: ElementBase {              \
		Op_##opcode(arg0_type arg0): arg0(arg0) {} \
		arg0_type arg0;                            \
	};
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                      \
	struct Op_##opcode: ElementBase {                                          \
		Op_##opcode(arg0_type arg0, arg1_type arg1): arg0(arg0), arg1(arg1) {} \
		arg0_type arg0;                                                        \
		arg1_type arg1;                                                        \
	};

#include <vm/program/opcodes_list.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

		/**
		 * @brief An extra instruction that represents a comment.
		 * @note It also helps with macro, because without it the template
		 * below would finish with a `,`, which does not compile.
		 */
		struct Comment: ElementBase {
			Comment() = default;

			Comment(base::StrID comment): comment(comment) {}

			base::StrID comment;
		};
	}

	using Instruction = std::variant<
#define HANDLE_OPCODE(opcode) VM_INSTR_FROM_NAME(opcode),
#include <vm/program/opcodes_list.hpp>
#undef HANDLE_OPCODE
		instructions::Comment>;

}
