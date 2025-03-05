#pragma once

#include <deque>
#include <ostream>
#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include "backends/vm/instructions.hpp"
#include "base/string_id.hpp"

namespace compiler::backend_vm {
	struct VmElement {
		virtual ~VmElement() = default;
	};

	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::deque<VmInstruction>;

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
		std::deque<vm::TypeOfData> types;
		std::deque<Function>       functions;
	};
}
