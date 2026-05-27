#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/local_stack_database.hpp>

namespace vm::code::valid_function {
	struct ValidFunction final: ElementBase {
		Identifier    name;
		CodeBlock     body;
		FuncSignature signature;
		LocalStackDb  local_stack;

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
