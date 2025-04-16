/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include "go_to_definition.hpp"
#include "base/anycast.hpp"
#include "helios/hout/elements/expr.hpp"
#include "helios/hout/elements/query_hout_of_expr.hpp"
#include "pst_parser/elements/hierarchy/not_statements.hpp"
#include "pst_parser/elements/hierarchy/statements.hpp"
#include "pst_parser/lang_parser_element.hpp"
#include "pst_parser/pst_visitor.hpp"
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

	base::MCRef<pst::LangElement> findElement(MCRef<pst::LangElement> element, usize offset) {
        for (auto sub: element->viewChildren()) {
			auto curr_position = sub->getSourcePosition();
			if (curr_position.getStart() <= offset && curr_position.getEnd() >= offset)
				return findElement(sub, offset);
		}
        return element;
	}

	Definition findDefinition(MCRef<pst::LangElement> element) {
		auto expr = query::entryPoint<compiler::helios::QueryHoutOfExpr>({element});
		
		// @todo
		// fix it
		compiler::helios::code::IdentifierExpr idexpr = expr;
		return {idexpr.symbol.ref.getPSTData().pst_element};
	}
}
