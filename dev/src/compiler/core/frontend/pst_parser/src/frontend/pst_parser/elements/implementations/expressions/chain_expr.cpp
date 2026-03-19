#include "../../hierarchy/expressions/chain_expr.hpp"

#include "../../hierarchy/expressions/access.hpp"  // IWYU pragma: keep
#include "../../hierarchy/expressions/atom.hpp"    // IWYU pragma: keep
#include "../../hierarchy/expressions/call.hpp"    // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	i64 ChainExpr::toNextLink(const LangParserState& state) {
		CORE_ASSERT(state.ctokens().size() > 0, "Illegal max length to next link");

		i64 fwd = 1;
		if (state[0].is(Keyword::Lambda)) fwd = 2;  // Skip ()
		PST_WHILE(!state[fwd].is(Token::Type::Sentinel)) {
			if (state[fwd].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
			    ).copyValueOr(false)
			    || state[fwd].isBracketGroup(lexer::Token::Square)
			    || state[fwd].isBracketGroup(lexer::Token::Round)) {
				break;
			}
			fwd++;
		}

		return fwd;
	}

	MBox<ExprElement> ChainExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 fwd = toNextLink(state);
		if (state[fwd].is(Token::Type::Sentinel)) return Lower::parse(state);

		auto out = makeBox<ChainExpr>(state);

		PARSE().autoFallbackLen(fwd).with(&out->atom, Lower::parse);

		PST_WHILE(state.ctokens().size() > 0) {
			fwd = toNextLink(state);
			MBox<ExprElement> extension;
			if (state[0].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
			    ).copyValueOr(false)) {
				PARSE().autoFallbackLen(fwd).with(&extension, Access::parse);
			} else if (state[0].isBracketGroup(lexer::Token::Round)
			           || state[0].isBracketGroup(lexer::Token::Square)) {
				PARSE().autoFallbackLen(fwd).with(&extension, Call::parse);
			} else {
				state.logInt(makeBox<BadChainExprError>(
					dia::SourcePosition(state.getPosition(), state.getPosition(fwd - 1).getEnd())
				));
			}
			if (extension) {
				out->chain.emplace_back(nullptr);
				PARSE().assign(&out->chain.back(), std::move(extension));
			}
		}

		PST_RETURN out;
	}

	void ChainExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("atom": )";
		nullAwareDprint(atom, out);
		out << R"(, "chain": [)";
		bool first = true;
		for (auto& link: chain) {
			if (!first)
				out << ", ";
			else
				first = false;
			nullAwareDprint(link, out);
		}
		out << "]";

		out << "}";
	}

	HashAlg& ChainExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, chain.size());
		return partial_hash;
	}

	void ChainExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitChainExpr(*this);
	}

	AccessLocked<ExprElement> ChainExpr::getAtom() const { return atom.give(); }

	void ChainExpr::calcElementPathHashRecursive() {
		auto path = getElementPathHash();
		calcNamedChildPath(atom, path);

		calcIndexedListChildPath<ExprElement>({ chain }, path);
	}
}
