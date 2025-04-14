/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include "go_to_definition.hpp"
#include "base/exceptions.hpp"
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

	std::vector<Definition> findDefinitions(MCRef<pst::LangElement> element) {
        // @todo
        // implement finding element definition
        CORE_PANIC("Finding definition for LSP is not implemented yet");
     }
}
