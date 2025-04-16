/**
 * @file go_to_definition.hpp
 * @brief Go to definition definition
 */

#pragma once

#include <pst_parser/lang_parser_element.hpp>
#include <pst_parser/pst.hpp>
#include <base/ref.hpp>

#include <string>
#include <utility>

namespace lsp {
	struct Definition {
		std::string             uri;
		std::pair<usize, usize> start;  // line, char
		std::pair<usize, usize> end;    // line, char

        std::string toJSON();
	};

    base::MCRef<pst::LangElement> findElement(MCRef<pst::LangElement> root, usize offset);
	Definition findDefinition(MCRef<pst::LangElement>);
}
