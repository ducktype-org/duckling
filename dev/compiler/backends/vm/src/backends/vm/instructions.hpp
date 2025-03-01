/**
 * @brief This file contains a list of instructions. This is temporary and should be integrated into
 * VM itself.
 */

#pragma once

#include <variant>
#include "../../../../../../VM/src/code_data/opcode_args.hpp"
#include "base/exceptions.hpp"

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

#include "../../../../../../VM/src/code_data/opcodes_list.hpp"

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

	/**
	 * @brief This type is here just to make templates work with our macros.
	 */
	struct Guardian {
		Guardian() { CORE_PANIC("Should not instantiate Guardian"); }
	};

	using VmInstruction = std::variant<
#define HANDLE_OPCODE(opcode) Op_##opcode,
#include "../../../../../../VM/src/code_data/opcodes_list.hpp"
#undef HANDLE_OPCODE
		Guardian>;
}
