#include "../../hierarchy/expressions/comparison_chain.hpp"

#include "../../hierarchy/expressions/general_binary.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	i64 ComparisonChain::skipToOp(const LangParserState& state, i64 base, i64 length) {
		i64 fwd = base;
		PST_WHILE(fwd < length && !ExprClassify::isComparison(state.ctokens(), fwd)) fwd++;
		return fwd;
	}

	MBox<ExprElement> ComparisonChain::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		i64 fwd = skipToOp(state, 0, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = makeBox<ComparisonChain>(state.getPosition());

		PST_WHILE(fwd < length) {
			out->sub_expr.emplace_back(nullptr);
			state.parse(out).with(&out->sub_expr.back(), Lower::parse, +fwd);

			out->operators.push_back(state[0].asBinaryOperator().value());
			state.parse(out).eatOne();

			length -= fwd + 1;
			fwd = skipToOp(state, 0, length);
		}

		out->sub_expr.emplace_back(nullptr);
		state.parse(out).with(&out->sub_expr.back(), Lower::parse, +fwd);

		PST_RETURN out;
	}

	void ComparisonChain::dprint(std::ostream& out) const {
		out << "{";

		out << R"("sub-expressions": [)";
		bool first = true;
		for (auto& expr: sub_expr) {
			if (!first)
				out << ", ";
			else
				first = false;
			nullAwareDprint(expr, out);
		}
		out << "]";

		out << "}";
	}

	LangElement::HashAlg& ComparisonChain::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, sub_expr.size());
		addToHash(partial_hash, operators);
		return partial_hash;
	}

	void ComparisonChain::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitComparisonChain(*this);
	}

	void ComparisonChain::calcElementPathHashRecursive() {
		calcIndexedListChildPath<ExprElement>({ sub_expr }, getElementPathHash());
	}
}
