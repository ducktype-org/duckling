/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include "go_to_definition.hpp"

#include <helios/symbols/simple.hpp>
#include <helios/utils/go_to_definition.hpp>
#include <pst_parser/lang_parser_element.hpp>
#include <pst_parser/pst.hpp>

#include <base/misc/optional.hpp>

#include <query_framework/utils/with_context_do.hpp>
#include <token_source/source.hpp>

#include <format>
#include <string>

namespace lsp {
	std::string Definition::toJSON() {
		constexpr std::string_view JSON_TEMPLATE
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
			JSON_TEMPLATE,
			this->uri,
			this->start.first,
			this->start.second,
			this->end.first,
			this->end.second
		);
	}

	Definition::Definition(const pst::LangElement* element) {
		auto source_position = element->getSourcePosition();
		this->uri            = source_position.getSource()->getFile().getFilePath().uri();
		this->start          = source_position.getStartLineColumn();
		this->end            = source_position.getEndLineColumn();
	}

	pst::AccessLocked<pst::LangElement> findElement(
		pst::AccessLocked<pst::LangElement> root, usize offset
	) {
		auto element = root.illegalAccess().value();

		for (auto sub: element->viewChildren()) {
			auto curr_position = sub.illegalAccess().value()->getSourcePosition();
			if (curr_position.getStart() <= offset && curr_position.getEnd() >= offset)
				return findElement(sub, offset);
		}
		return element;
	}

	base::Optional<Definition> findDefinition(pst::AccessLocked<pst::LangElement> element) {
		auto pst_expr = element.dynamicCast<pst::ExprElement>();

		if (pst_expr.illegalAccess().empty()) return {};

		base::Optional<Definition> result;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto sym_id = compiler::helios::querySymIDOfPSTExpr(ctx, pst_expr);
			if (sym_id.has_value()) {
				auto stmt = compiler::helios::stmt(ctx, sym_id.value());
				result    = Definition{ &*stmt.value() };
			}
		});

		return result;
	}
}
