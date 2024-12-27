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
		while (fwd < length) {
			if (state[fwd].is(lang_def::NamedOperator::Period)) break;
			if (state[fwd].isBracketGroup(lexer::Token::Square)) break;
			if (state[fwd].isBracketGroup(lexer::Token::Round)) break;
			fwd++;
		}

		return fwd;
	}

	MBox<ExprElement> ChainExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		i64 fwd = toNextLink(state, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = makeBox<ChainExpr>(state.getPosition());

		state.parse(out).with(&out->literal, Lower::parse, +fwd);
		length -= fwd;

		while (length > 0) {
			fwd = toNextLink(state, length);
			out->chain.emplace_back(nullptr);
			if (state[0].is(lang_def::NamedOperator::Period)) {
				state.parse(out).with(&out->chain.back(), Access::parse, +fwd);
			} else if (state[0].isBracketGroup(lexer::Token::Round) || state[0].isBracketGroup(lexer::Token::Square)) {
				state.parse(out).with(&out->chain.back(), Call::parse, +fwd);
			} else {
				state.log(base::make_unique<BadChainExprError>(
					dia::SourcePosition(state.getPosition(), state.getPosition(fwd - 1).getEnd())
				));
				fastForward(state, fwd);
			}
			length -= fwd;
		}

		return out;
	}

	void ChainExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("literal": )";
		nullAwareDprint(literal, out);
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

	void ChainExpr::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitChainExpr(*this); }

	MCRef<ExprElement> BinaryOperator::getLeftOperand() const { return left.ref(); }

	MCRef<ExprElement> BinaryOperator::getRightOperand() const { return right.ref(); }

	lexer::Operator BinaryOperator::getOperator() const { return op; }

	const std::vector<MBox<ExprElement>>& Comma::getExpressions() const { return expressions; }

	MCRef<ExprElement> ChainExpr::getLiteral() const { return literal.ref(); }

	const std::vector<MBox<ExprElement>>& ChainExpr::getChain() const { return chain; }

	base::StrID Access::getType() const { return type; }

	const tpc::Identifier& Access::getName() const { return name; }

	lexer::Operator SuffixOperator::getOperator() const { return op; }

	MCRef<ExprElement> SuffixOperator::getExpr() const { return expr.ref(); }

	lexer::Operator PrefixOperator::getOperator() const { return op; }

	MCRef<ExprElement> PrefixOperator::getExpr() const { return expr.ref(); }
}
