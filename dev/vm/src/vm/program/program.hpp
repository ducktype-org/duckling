#pragma once

#include <vm/preprocessor/parser/type_of_data.hpp>
#include "instructions.hpp"
#include <base/string_id.hpp>

namespace vm::program {
	struct VmElement {
		virtual ~VmElement() = default;
	};

	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<VmInstruction>;

	/**
	 * @brief Represents bytecode a function.
	 */
	struct Function: public VmElement {
		base::StrID name;
		usize       stack_size    = 0;
		usize       arg_size      = 0;
		usize       next_arg_size = 0;
		usize       ret_size      = 0;

		CodeBlock body;
	};

	/**
	 * @brief Represents a file. File may contain multiple
	 * functions and type definitions.
	 */
	struct CodeFile: public VmElement {
		std::vector<vm::parser::TypeOfData> types;
		std::vector<Function>               functions;
	};
}
