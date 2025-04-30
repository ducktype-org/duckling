#pragma once

#include "instructions.hpp"

#include <base/string_id.hpp>

#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace vm::code {
	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<Instruction>;

	/**
	 * @brief Represents bytecode a function.
	 */
	struct Function final: ElementBase {
		base::StrID name;
		// @TODO this should be the compiler's responsibility, move it there
		usize                           local_stack_size = 0;
		base::HashMap<base::StrID, i64> local_offset_map;

		CodeBlock body;
	};

	struct CodeCollection final {
		std::vector<Function>   functions;
		std::vector<TypeOfData> types;
	};
}
