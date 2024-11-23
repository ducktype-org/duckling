#include "preamble.hpp"

namespace pst::expr {
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

	ParserRef<ExprElement> ChainExpr::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Chain Expression" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		i64 fwd = toNextLink(state, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = base::make_unique<ChainExpr>(state.getPosition());

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
				// Error
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

	void ChainExpr::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitChainExpr(*this); }

	ParserCBorrowRef<ExprElement> BinaryOperator::getLeftOperand() const { return left.borrow(); }

	ParserCBorrowRef<ExprElement> BinaryOperator::getRightOperand() const { return right.borrow(); }

	lexer::Operator BinaryOperator::getOperator() const { return op; }

	const std::vector<ParserRef<ExprElement>>& Comma::getExpressions() const { return expressions; }

	ParserCBorrowRef<ExprElement> ChainExpr::getLiteral() const { return literal.borrow(); }

	const std::vector<ParserRef<ExprElement>>& ChainExpr::getChain() const { return chain; }

	base::StrID Access::getType() const { return type; }

	const tpc::Identifier& Access::getName() const { return name; }

	lexer::Operator SuffixOperator::getOperator() const { return op; }

	ParserCBorrowRef<ExprElement> SuffixOperator::getExpr() const { return expr.borrow(); }

	lexer::Operator PrefixOperator::getOperator() const { return op; }

	ParserCBorrowRef<ExprElement> PrefixOperator::getExpr() const { return expr.borrow(); }
}
