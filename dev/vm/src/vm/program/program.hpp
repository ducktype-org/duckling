#pragma once

#include <vm/preprocessor/parser/type_of_data.hpp>
#include "base/maps.hpp"
#include "instructions.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include <base/string_id.hpp>

namespace vm::program {
	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<VmInstruction>;

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
		std::vector<vm::parser::TypeOfData> types;
		std::vector<Function>               functions;
	};

	/**
	 * @todo Hide this. This is temporarily exposed.
	 */
	struct Program {
		base::HashMap<base::StrID, Function>               functions{};
		base::HashMap<base::StrID, vm::parser::TypeOfData> types{};
		Box<TypeMetadata>                                  type_metadata = makeBox<TypeMetadata>();
	};
}
