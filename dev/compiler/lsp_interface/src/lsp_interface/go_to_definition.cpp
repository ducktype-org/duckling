/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include "go_to_definition.hpp"
#include <base/exceptions.hpp>
#include <query_framework/query_entry_point.hpp>
#include <helios/queries.hpp>
#include <format>
#include <string>

namespace lsp {
	std::string Definition::toJSON() {
		constexpr std::string_view json_template
			= "uri: {},\n"
			  "range: {{\n"
			  "    start: {{\n"
			  "        line: {},\n"
			  "        character: {}\n"
			  "    }},\n"
			  "    end: {{\n"
			  "        line: {},\n"
			  "        character: {}\n"
			  "    }}\n"
			  "}}\n";

		return std::format(
			json_template,
			this->uri,
			this->start.first,
			this->start.second,
			this->end.first,
			this->end.second
		);
	}

	base::MCRef<pst::LangElement> findElement(CRef<pst::LangElement> element, usize offset) {
        for (auto sub: element->viewChildren()) {
			auto curr_position = sub->getSourcePosition();
			if (curr_position.getStart() <= offset && curr_position.getEnd() >= offset)
				return findElement(sub, offset);
		}
        return element;
	}

	Definition findDefinition(MCRef<pst::LangElement> element) {
		// @todo
		// implement finding element definition
		CORE_PANIC("Finding definition for LSP is not implemented yet");
	}
}
