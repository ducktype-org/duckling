#pragma once

#include "diagnostic_interactive/core/diagnostic_file.hpp"
#include "frontend/pst_parser/access.hpp"
#include "frontend/pst_parser/elements/hierarchy/expr_holders.hpp"
#include "frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp"
#include "frontend/pst_parser/elements/hierarchy/statements/alias.hpp"
#include "helios_private/lookup/interface.hpp"
#include "helios_private/scopes/scopes.hpp"
#include "helios_private/symbols/symbol_data.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/usage.hpp>
#include <helios/symbols/simple.hpp>

#include "diagnostic/source_position.hpp"
#include "query_framework/context.hpp"
#include <query_framework/utils/with_context_do.hpp>

#include <functional>
#include <utility>
#include <vector>

namespace compiler::helios::errors {
	using namespace dia_int;

	class IsAliasNote: public MessageBase {
		Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "note", .family = "lookup", .name = "is_alias"
			};
		}

	public:
		IsAliasNote(std::string alias_name, std::string underlying_name): MessageBase() {
			addArgument<TextArgument>("alias_name", std::move(alias_name));
			addArgument<TextArgument>("underlying_name", std::move(underlying_name));
		}
	};

	inline std::string getStr(dia::SourcePosition pos) {
		return pos.getSource()->getCharRange(pos.getStart(), pos.getEnd()).stdString();
	}

	void checkForAliases(
		query::Context&               ctx,
		MessageBase&                  msg,
		pst::Access<pst::ExprElement> elem,
		std::vector<std::string>&     linked_messages
	);

	class InteractiveType: public InteractiveElement {
		tsh::SymbolType<>                             symbol_type;
		base::Optional<pst::Access<pst::ExprElement>> pst_expr;

		Box<dia_file::Component> build(MessageBase& msg) final {
			std::string              displayed_name;
			std::vector<std::string> linked_messages;

			if (pst_expr.has_value()) {
				displayed_name = getStr(pst_expr.value()->getSourcePosition());
				query::utils::withContextDo([&](query::Context& ctx) {
					checkForAliases(ctx, msg, pst_expr.value(), linked_messages);
				});
			} else
				displayed_name = symbol_type.toString();

			msg.addEntity<TextBasedEntity>(linked_messages, displayed_name);

			auto content = makeBox<dia_file::TextComponent>(displayed_name);
			auto link
				= makeBox<dia_file::LinkComponent>(std::move(linked_messages), std::move(content));
			return link;
		}

	public:
		InteractiveType(
			tsh::SymbolType<> symbol_type, base::Optional<pst::Access<pst::ExprElement>> pst_expr
		):
			  symbol_type(symbol_type),
			  pst_expr(std::move(pst_expr)) {}
	};

	class VariableElement {};

	class IncompatibleTypesError: public MessageWithCodeFragmentAndCause {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "incompatible_types" };
		}

	public:
		IncompatibleTypesError(
			dia::SourcePosition source_position,
			InteractiveType     expected_type,
			InteractiveType     actual_type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<InteractiveArgument>(
				"expected_type", makeBox<InteractiveType>(expected_type)
			);
			addArgument<InteractiveArgument>("given_type", makeBox<InteractiveType>(actual_type));
		}
	};
}
