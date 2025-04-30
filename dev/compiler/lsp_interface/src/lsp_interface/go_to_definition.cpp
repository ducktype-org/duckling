/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include "go_to_definition.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/queries.hpp>
#include <helios/query_hout_of_expr.hpp>
#include <helios/symbols/simple.hpp>
#include <pst_parser/lang_parser_element.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include "base/ref.hpp"
#include <base/exceptions.hpp>
#include <base/optional.hpp>

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

	Definition::Definition(const pst::LangElement * element) {
		auto source_position = element->getSourcePosition();
		this->uri            = source_position.getSource()->getPath().uri();
		this->start          = source_position.getStartLineColumn();
		this->end            = source_position.getEndLineColumn();
	}

	pst::AccessLocked<pst::LangElement> findElement(
		pst::AccessLocked<pst::LangElement> locked_element, usize offset
	) {
		auto element = locked_element.illegalAccess().value();

		for (auto sub: element->viewChildren()) {
			auto curr_position = sub.illegalAccess().value()->getSourcePosition();
			if (curr_position.getStart() <= offset && curr_position.getEnd() >= offset)
				return findElement(sub, offset);
		}
		return element;
	}

	base::Optional<Definition> findDefinition(pst::AccessLocked<pst::LangElement> element) {
		auto expr = query::entryPoint<compiler::helios::QueryHoutOfExpr>({ MCRef<pst::ExprElement>(
			dynamic_cast<const pst::ExprElement*>(&*element.illegalAccess().value())
		) });

		if (!expr.hasValue()) return {};

		auto id_expr = dynamic_cast<compiler::helios::code::IdentifierExpr*>(&*expr.value());

		base::Optional<Definition> result;

		query::utils::withContextDo([&](query::Context& context) {
			auto stmt = compiler::helios::stmt(context, id_expr->symbol);
			result = Definition(dynamic_cast<const pst::LangElement*>(&*stmt.value()));
		});

		return result;
	}
}
