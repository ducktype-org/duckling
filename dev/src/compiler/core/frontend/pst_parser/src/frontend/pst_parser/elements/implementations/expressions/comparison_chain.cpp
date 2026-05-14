#include "../../hierarchy/expressions/comparison_chain.hpp"

#include "../../hierarchy/expressions/general_binary.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	i64 ComparisonChain::skipToOp(const LangParserState& state, i64 base) {
		i64 fwd = base;
		PST_WHILE(
			!state[fwd].is(Token::Type::Sentinel)
			&& !ExprClassify::isComparison(state.ctokens(), fwd)
		)
		fwd++;
		return fwd;
	}

	MBox<ExprElement> ComparisonChain::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		i64 fwd = skipToOp(state, 0);
		if (fwd == length) return Lower::parse(state);

		// A chain with only one comparison operator should be returned as a binary operator
		// because the generated code is much simpler that way.
		if (skipToOp(state, fwd + 1) == length) {
			auto op  = state[fwd].asBinaryOperator().value();
			auto out = makeBox<GeneralBinary>(state, op);

			PARSE().autoFallbackLen(fwd).with(&out->left, Lower::parse);
			PARSE().one(op);
			PARSE().autoFallbackLen(length - fwd - 1).with(&out->right, Lower::parse);

			PST_RETURN out;
		}

		auto out = makeBox<ComparisonChain>(state);

		PST_WHILE(fwd < length) {
			out->sub_expr.emplace_back(nullptr);
			PARSE().autoFallbackLen(fwd).with(&out->sub_expr.back(), Lower::parse);

			out->operators.push_back(state[0].asBinaryOperator().value());
			PARSE().eatOne();

			length -= fwd + 1;
			fwd = skipToOp(state, 0);
		}

		out->sub_expr.emplace_back(nullptr);
		PARSE().with(&out->sub_expr.back(), Lower::parse);

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

	HashAlg& ComparisonChain::addElementDataToStableHash(HashAlg& partial_hash) const {
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
