#include "interactive_errors.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/alias.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>

namespace compiler::helios {
	using namespace dia_int;

	std::string getStr(dia::SourcePosition pos) {
		return pos.getSource()->getCharRange(pos.getStart(), pos.getEnd() + 1).stdString();
	}

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

	class IsAliasCodeNote: public MessageWithCodeFragmentAndCause {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "is_alias_code" };
		}

	public:
		IsAliasCodeNote(dia::SourcePosition pos, std::string alias_name, std::string underlying_name):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<TextArgument>("alias_name", std::move(alias_name));
			addArgument<TextArgument>("underlying_name", std::move(underlying_name));
		}
	};

	void checkForAliases(
		query::Context&               ctx,
		MessageBase&                  msg,
		pst::Access<pst::LangElement> elem,
		std::vector<std::string>&     linked_messages
	) {
		auto ident_opt = elem.dynamicCast<pst::expr::IdentifierLiteral>();
		if (!ident_opt.has_value()) return;
		auto       ident = ident_opt.value();
		const auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ ident });
		const auto lookup_result
			= HInterface::ofScopeWithParents(scope).lookup(ctx, ident->getName().value);

		std::function<void(const LookupResult&, const std::string&)> emit_alias_note
			= [&](const LookupResult& current, const std::string& alias_name) {
				  if (current.children.size() != 1) return;

				  auto nested = current.children[0];
				  if (kind(nested.node) == SymbolKind::Alias) {
					  auto alias_stmt = getSymRef(nested.node)
				                            ->getPSTData()
				                            ->pst_element.unlock(ctx)
				                            .dynamicCast<pst::Alias>()
				                            .value();
					  auto underlying_chain
						  = getStr(alias_stmt->getPointed().unlock(ctx)->getSourcePosition());

					  auto id = MessageBase::getUniqueID();
					  msg.addLinkedMessage(
						  id,
						  makeBox<IsAliasCodeNote>(
							  alias_stmt->getSourcePosition(), alias_name, underlying_chain
						  )
					  );
					  linked_messages.push_back(std::move(id));

					  emit_alias_note(nested.inner, underlying_chain);
				  };
			  };

		emit_alias_note(*lookup_result, ident->getName().value.str());
	}

	Box<dia_args::Component> InteractiveType::getValue(MessageBase& msg) {
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

		auto content = makeBox<dia_args::TextComponent>(displayed_name);
		auto link
			= makeBox<dia_args::LinkComponent>(std::move(linked_messages), std::move(content));
		return link;
	}

	class FunctionDeclaredHereNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "function_declared_here" };
		}

	public:
		FunctionDeclaredHereNote(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	Box<dia_args::Component> InteractiveFunction::getValue(MessageBase& msg) {
		std::string              message_id;
		std::vector<std::string> linked_messages;
		std::string              displayed_name = name(function_symbol).str();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto maybe_function = getSymRef(function_symbol)->getPSTData()->pst_element.unlock(ctx);
			if (maybe_function->getElementKind() == pst::ElementKind::Fun) {
				auto function  = maybe_function.dynamicCast<pst::Fun>().value();
				auto fun_decl  = function->getParams().unlock(ctx);
				auto fun_ident = function->getNameIdentifier();
				auto position
					= dia::SourcePosition::merge(fun_ident.position, fun_decl->getSourcePosition());
				auto id = MessageBase::getUniqueID();
				msg.addLinkedMessage(id, makeBox<FunctionDeclaredHereNote>(position));
				linked_messages.push_back(std::move(id));
			}
		});

		msg.addEntity<TextBasedEntity>(linked_messages, displayed_name);

		auto content = makeBox<dia_args::TextComponent>(displayed_name);
		auto link
			= makeBox<dia_args::LinkComponent>(std::move(linked_messages), std::move(content));
		return link;
	}
}
