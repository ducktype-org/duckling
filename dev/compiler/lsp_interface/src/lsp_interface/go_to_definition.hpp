/**
 * @file go_to_definition.hpp
 * @brief Go to definition definition
 */

#pragma once

#include <pst_parser/pst.hpp>
#include <base/ref.hpp>

#include <string>
#include <utility>
#include <vector>

namespace lsp {
	struct Definition {
		std::string             uri;
		std::pair<usize, usize> start;  // line, char
		std::pair<usize, usize> end;    // line, char

        std::string toJSON();
	};

	std::vector<Definition> findDefinitions(MCRef<pst::LangElement>);
}
