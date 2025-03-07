/**
 * @brief This file contains a list of structures representing DVM opcodes.
 */

#pragma once

#include <variant>
#include <code_data/opcode_args.hpp>
#include <base/exceptions.hpp>

namespace compiler::backend_vm {
#define HANDLE_OPCODE_0ARGS(opcode) \
	struct Op_##opcode {};
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) \
	struct Op_##opcode {                       \
		arg0_type arg0;                        \
	};
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	struct Op_##opcode {                                  \
		arg0_type arg0;                                   \
		arg1_type arg1;                                   \
	};

#include <code_data/opcodes_list.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

	/**
	 * @brief An extra instruction that represents a comment.
	 * @note It also helps with macro, because without it the template
	 * below would finish with a `,`, which does not compile.
	 */
	struct Comment {
		base::StrID comment;
	};

	using VmInstruction = std::variant<

#define HANDLE_OPCODE(opcode) Op_##opcode,
#include <code_data/opcodes_list.hpp>
#undef HANDLE_OPCODE
		Comment>;
}
