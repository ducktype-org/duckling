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

		i64 fwd = skipToOp(state, 0);
		if (fwd == state.ctokens().size()) return Lower::parse(state);

		auto out = makeBox<ComparisonChain>(state);

		PST_WHILE(fwd < state.ctokens().size()) {
			out->sub_expr.emplace_back(nullptr);
			PARSE().autoFallbackLen(fwd).with(&out->sub_expr.back(), Lower::parse);

			out->operators.emplace_back(nullptr);
			PARSE().autoFallbackLen(1UL).with(&out->operators.back(), OperatorWrapper::parse);

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
		out << "],";

		out << R"("operators": [)";
		first = true;
		for (auto& op: operators) {
			if (!first)
				out << ", ";
			else
				first = false;
			nullAwareDprint(op, out);
		}
		out << "]";

		out << "}";
	}

	HashAlg& ComparisonChain::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, sub_expr.size());
		addToHash(partial_hash, operators.size());
		return partial_hash;
	}

	void ComparisonChain::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitComparisonChain(*this);
	}

	void ComparisonChain::calcElementPathHashRecursive() {
		auto path          = getElementPathHash();
		auto sub_expr_path = hashing::ComponentHash(path, "value");
		;
		calcIndexedListChildPath<ExprElement>({ sub_expr }, sub_expr_path);
		auto op_path = hashing::ComponentHash(path, "operator");
		;
		calcIndexedListChildPath<OperatorWrapper>({ operators }, op_path);
	}
}
