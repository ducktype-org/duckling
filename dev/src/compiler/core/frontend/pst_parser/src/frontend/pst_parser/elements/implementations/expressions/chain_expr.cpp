#include "../../hierarchy/expressions/chain_expr.hpp"

#include "../../hierarchy/expressions/access.hpp"  // IWYU pragma: keep
#include "../../hierarchy/expressions/atom.hpp"    // IWYU pragma: keep
#include "../../hierarchy/expressions/call.hpp"    // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	class BadChainExprError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected access or call expression expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadChainExprError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	i64 ChainExpr::toNextLink(const LangParserState& state, i64 length) {
		CORE_ASSERT(length > 0, "Illegal max length to next link");

		i64 fwd = 1;
		if (state[0].is(Keyword::Lambda)) fwd = 2;  // Skip ()
		PST_WHILE(fwd < length) {
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

	MBox<ExprElement> ChainExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		i64 fwd = toNextLink(state, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = makeBox<ChainExpr>(state.getPosition());

		state.parse(out).with(&out->atom, Lower::parse, +fwd);
		length -= fwd;

		PST_WHILE(length > 0) {
			fwd = toNextLink(state, length);
			MBox<ExprElement> extension;
			if (state[0].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
			    ).copyValueOr(false)) {
				state.parse(out).with(&extension, Access::parse, +fwd);
			} else if (state[0].isBracketGroup(lexer::Token::Round)
			           || state[0].isBracketGroup(lexer::Token::Square)) {
				state.parse(out).with(&extension, Call::parse, +fwd);
			} else {
				state.log(makeBox<BadChainExprError>(
					dia::SourcePosition(state.getPosition(), state.getPosition(fwd - 1).getEnd())
				));
				fastForward(state, fwd);
			}
			if (extension) {
				out->chain.emplace_back(nullptr);
				state.parse(out).assign(&out->chain.back(), std::move(extension));
			}
			length -= fwd;
		}

		return out;
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

	LangElement::HashAlg& ChainExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
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
