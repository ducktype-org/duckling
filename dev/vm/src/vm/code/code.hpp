#pragma once

#include <vm/code/type_of_data.hpp>
#include "instructions.hpp"
#include <vm/code/element_base.hpp>
#include <base/string_id.hpp>

namespace vm::code {
	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<Instruction>;

	/**
	 * @brief Represents bytecode a function.
	 */
	struct Function: ElementBase {
		base::StrID name;
		usize       local_stack_size = 0;
		usize       arg_size         = 0;
		usize       next_arg_size    = 0;
		usize       ret_size         = 0;

		CodeBlock body;
	};
}
