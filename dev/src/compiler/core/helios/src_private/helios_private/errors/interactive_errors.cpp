#include "interactive_errors.hpp"

#include "diagnostic_interactive/usage.hpp"

#include "diagnostic/source_position.hpp"

namespace compiler::helios::errors {

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
		pst::Access<pst::ExprElement> elem,
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

	Box<dia_file::Component> InteractiveType::getValue(MessageBase& msg) {
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

	IncompatibleTypesError::IncompatibleTypesError(
		dia::SourcePosition source_position,
		InteractiveType     expected_type,
		InteractiveType     actual_type
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<InteractiveArgument>("expected_type", makeBox<InteractiveType>(expected_type));
		addArgument<InteractiveArgument>("given_type", makeBox<InteractiveType>(actual_type));
	}

	InteractiveType::InteractiveType(
		tsh::SymbolType<> symbol_type, base::Optional<pst::Access<pst::ExprElement>> pst_expr
	):
		  symbol_type(symbol_type),
		  pst_expr(std::move(pst_expr)) {}
}
