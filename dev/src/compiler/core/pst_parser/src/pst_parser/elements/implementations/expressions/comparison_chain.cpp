#include "../../hierarchy/expressions/comparison_chain.hpp"

#include "../../hierarchy/expressions/general_binary.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	i64 ComparisonChain::skipToOp(const LangParserState& state, i64 base, i64 length) {
		i64 fwd = base;
		while (fwd < length && !ExprClassify::isComparison(state, fwd)) fwd++;
		return fwd;
	}

	MBox<ExprElement> ComparisonChain::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		i64 fwd = skipToOp(state, 0, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = makeBox<ComparisonChain>(state.getPosition());

		while (fwd < length) {
			out->sub_expr.emplace_back(nullptr);
			state.parse(out).with(&out->sub_expr.back(), Lower::parse, +fwd);

			out->operators.push_back(state[0].asBinaryOperator().value());
			state.parse(out).eatOne();

			length -= fwd + 1;
			fwd = skipToOp(state, 0, length);
		}

		out->sub_expr.emplace_back(nullptr);
		state.parse(out).with(&out->sub_expr.back(), Lower::parse, +fwd);

		return out;
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

	void ComparisonChain::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitComparisonChain(*this);
	}

	void ComparisonChain::calcElementPathsRecursive() {
		calcIndexedListChildPath(sub_expr, getElementPath());
	}
}
