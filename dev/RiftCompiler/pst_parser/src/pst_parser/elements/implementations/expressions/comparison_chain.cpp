#include "preamble.hpp"

namespace pst::expr {
	u64 ComparisonChain::skipToOp(const RiftParserState& state, u64 base, u64 length) {
		u64 fwd = base;
		while (fwd < length && !ExprClassify::isComparison(state, fwd)) fwd++;
		return fwd;
	}

	ParserRef<ExprElement> ComparisonChain::parse(RiftParserState& state, u64 length) {
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

			length -= fwd - 1;
			fwd = skipToOp(state, 0, length);
		}

		out->sub_expr.push_back(nullptr);
		state.parse(out).with(&out->sub_expr.back(), Lower::parse, +fwd);

		return out;
	}
}
