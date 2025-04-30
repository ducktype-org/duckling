/**
 * @file go_to_definition.hpp
 * @brief Go to definition definition
 */

#pragma once

#include <pst_parser/access.hpp>
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

		Definition(const pst::LangElement*);
	};

	pst::AccessLocked<pst::LangElement> findElement(pst::AccessLocked<pst::LangElement>, usize);
	base::Optional<Definition>          findDefinition(pst::AccessLocked<pst::LangElement>);
}
