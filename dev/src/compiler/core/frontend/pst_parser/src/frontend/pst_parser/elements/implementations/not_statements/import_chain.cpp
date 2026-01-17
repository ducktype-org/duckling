#include "preamble.hpp"

#include "../../hierarchy/not_statements/import_chain.hpp"
#include "../../hierarchy/not_statements/import_chains/import_identifier_as.hpp"
#include "../../hierarchy/not_statements/import_chains/import_nested.hpp"
#include "../../hierarchy/not_statements/import_chains/import_star_hides.hpp"

namespace pst {
	MBox<ImportChain> ImportChain::parse(LangParserState& state) {
		i64 fwd = 0;
		PST_WHILE(true) {
			if (state[fwd].isIdentifier() && state[fwd + 1].is(NamedOperator::Period)) {
				fwd+=2;
			} else {
				break;
			}
		}
		if (state[fwd].isIdentifier() && state[fwd + 1].is(NamedOperator::PeriodStar)) return ImportStarHides::parse(state);
		if (state[fwd].isIdentifier()) return ImportIdentifierAs::parse(state);
		if (state[fwd].isBracketGroup(Token::BracketType::Round)) return ImportNested::parse(state);

		state.logInt(makeBox<BadImportChainError>(state[fwd].getPosition()));		
		return nullptr;
	}
}