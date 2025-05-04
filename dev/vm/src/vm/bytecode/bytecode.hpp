#pragma once

#include "instructions.hpp"

#include <base/string_id.hpp>

#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

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
		CodeBlock   body;
	};

	struct CodeCollection final {
		std::vector<Function>   functions;
		std::vector<TypeOfData> types;
	};
}
