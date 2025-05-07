#pragma once

#include "instructions.hpp"

#include <token_parser_core/common_elements.hpp>

#include <base/string_id.hpp>

#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief Represents an identifier (e.g. symbol name) as string with ElementBase
	 * (SourcePosition).
	 */
	struct Identifier: ElementBase {
		Identifier() = default;

		Identifier(base::StrID str): str(str) {}

		Identifier(tpc::Identifier tpc_identifier):
			  ElementBase(tpc_identifier.position),
			  str(tpc_identifier.value) {}

		base::StrID str;

		bool operator==(const Identifier& other) const noexcept { return str == other.str; }

		operator base::StrID() const { return str; }
	};

	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<Instruction>;

	/**
	 * @brief Represents global data, like a constant or a variable.
	 */
	struct GlobalData {
		Identifier name;
		Identifier type;
	};

	/**
	 * @brief Represents bytecode a function.
	 */
	struct Function final: ElementBase {
		Identifier name;
		CodeBlock   body;
	};

	struct CodeCollection final {
		std::vector<Function>   functions;
		std::vector<TypeOfData> types;
		std::vector<GlobalData> global_data;
	};
}
