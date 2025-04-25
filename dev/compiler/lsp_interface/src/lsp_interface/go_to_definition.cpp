/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include "go_to_definition.hpp"
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/query_hout_of_expr.hpp>
#include <helios/symbols/symbols.hpp>
#include <pst_parser/lang_parser_element.hpp>
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

	Definition::Definition(CRef<pst::LangElement> element) {
		auto source_position = element->getSourcePosition();
		this->uri            = source_position.getSource()->getPath().uri();
		this->start          = source_position.getStartLineColumn();
		this->end            = source_position.getEndLineColumn();
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
		auto expr = query::entryPoint<compiler::helios::QueryHoutOfExpr>(
						{ dynamic_cast<const pst::ExprElement*>(&*element.toOpt().value()) }
		)
		                .value();
		auto id_expr = dynamic_cast<compiler::helios::code::IdentifierExpr*>(&*expr);
		return { compiler::helios::stmt(id_expr->symbol) };
	}
}
