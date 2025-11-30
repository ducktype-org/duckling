#include "interactive_errors.hpp"

namespace compiler::helios::errors {

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

        std::cout << "Lookup result children size: " << lookup_result->children.size() << "\n";
        std::cout << "Lookup result leaves size: " << lookup_result->leaves.size() << "\n";
        std::cout << name(lookup_result->leaves[0]).strView() << "\n";
        std::cout << (SymbolKind::Alias ==  kind(lookup_result->leaves[0])) << "\n";

		std::function<void(const LookupResult&, const std::string&)> emit_alias_note
			= [&](const LookupResult& current, const std::string& alias_name) {
                    std::cout << "Childre size: " << current.children.size() << "\n";
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
					  msg.addLinkedMessage(id, makeBox<IsAliasNote>(alias_name, underlying_chain));
					  linked_messages.push_back(std::move(id));

					  emit_alias_note(nested.inner, underlying_chain);
				  };
			  };

		emit_alias_note(*lookup_result, ident->getName().value.str());
	}
}
