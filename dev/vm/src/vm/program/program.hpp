#pragma once

#include <vm/program/type_of_data.hpp>
#include "base/stable_type_id_name_map.hpp"
#include "instructions.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include <base/string_id.hpp>

namespace vm::program {
	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<Instruction>;

	/**
	 * @brief Represents bytecode a function.
	 */
	struct Function {
		base::StrID name;
		usize       local_stack_size = 0;
		usize       arg_size         = 0;
		usize       next_arg_size    = 0;
		usize       ret_size         = 0;

		CodeBlock body;
	};

	/**
	 * @brief Represents a file. File may contain multiple
	 * functions and type definitions.
	 */
	struct CodeFile {
		std::vector<TypeOfData> types;
		std::vector<Function>   functions;
	};

	/**
	 * @todo Hide this. This is temporarily exposed.
	 */
	struct Program {
		base::StableTypeIdNameMap<Function, usize> functions{};
		TypeMetadata                               types{};
	};
}
