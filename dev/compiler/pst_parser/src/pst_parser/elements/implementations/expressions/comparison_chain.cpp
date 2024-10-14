#include "preamble.hpp"

namespace pst::expr {
	u64 ComparisonChain::skipToOp(const LangParserState& state, u64 base, u64 length) {
		u64 fwd = base;
		while (fwd < length && !ExprClassify::isComparison(state, fwd)) fwd++;
		return fwd;
	}

	ParserRef<ExprElement> ComparisonChain::parse(LangParserState& state, u64 length) {
		std::cerr << "Parsing Comparison Chain Expression" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		u64 fwd = skipToOp(state, 0, length);
		if (fwd == length) return Lower::parse(state, length);

		auto out = base::make_unique<ComparisonChain>(state.getPosition());

		while (fwd < length) {
			out->sub_expr.push_back(nullptr);
			state.parse(out).with(&out->sub_expr.back(), Lower::parse, +fwd);

			out->operators.push_back(state[0].asOperator());
			state.parse(out).eatOne();

			length -= fwd + 1;
			fwd = skipToOp(state, 0, length);
		}

		out->sub_expr.push_back(nullptr);
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
}
