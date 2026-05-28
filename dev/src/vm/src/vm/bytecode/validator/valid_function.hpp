#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/local_stack_database.hpp>

namespace vm::code::valid_function {
	struct ValidFunction final: ElementBase {
		Identifier                name;
		CodeBlock                 body;
		std::vector<StackStateID> stack_states;
		FuncSignature             signature;
		LocalStackDb              local_stack;

		/**
		 * @brief constructs a normal (not validated) function, which from a valid function
		 * @warning THIS FUNCTION IS O(n) - consider using it 
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
