#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/flags.hpp>
#include <vm/bytecode/validator/local_stack_database.hpp>

namespace vm::code::valid_function {
	struct ValidFunction final: ElementBase {
		Identifier                name;
		CodeBlock                 body;
		std::vector<StackStateID> stack_states;  // One state for each instruction
		FuncSignature             signature;
		LocalStackDb              local_stack;
		InstructionFlag           flags;

		/**
		 * @brief constructs a normal (not validated) function, which from a valid function
		 * @warning THIS FUNCTION IS O(n) - USE IT CAREFULLY.
		 */
		Function toNormal() const {
			Function new_func;

			new_func.bytecode_pos = bytecode_pos;
			new_func.name         = name;
			new_func.body         = body;
			new_func.signature    = signature;

			return new_func;
		}
	};
}

namespace vm::code {
	using FunctionMap = ObjIdNameMap<valid_function::ValidFunction>;
}
