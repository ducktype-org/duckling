#include "../../hierarchy/expressions/comma.hpp"

#include "../../hierarchy/expressions/ternary.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(Comma, expressions);

	MBox<ExprElement> Comma::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		u64 length = state.ctokens().size();

		std::vector<i64> ends;
		for (i64 i = 0; i < length; i++)
			if (state[i].is(Special::Comma)) ends.push_back(i);

		if (ends.empty()) return Lower::parse(state);

		auto out   = makeBox<Comma>(state);
		i64  start = -1;

		for (auto end: ends) {
			out->expressions.emplace_back();
			PARSE().autoFallbackLen(end - 1 - start).with(&out->expressions.back(), Lower::parse);
			state.parse(out).one(Special::Comma);
			start = end;
		}
		if (start + 1 != length) {
			out->expressions.emplace_back();
			PARSE().with(&out->expressions.back(), Lower::parse);
		}
		PST_RETURN out;
	}

	void Comma::dprint(std::ostream& out) const {
		out << "{";

		out << R"("sub-expressions": [)";
		bool first = true;
		for (auto& sub_expr: expressions) {
			if (!first)
				out << ", ";
			else
				first = false;
			nullAwareDprint(sub_expr, out);
		}
		out << "]";

		out << "}";
	}

	HashAlg& Comma::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, expressions.size());
		return partial_hash;
	}

	void Comma::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitComma(*this); }

	void Comma::calcElementPathHashRecursive() {
		calcIndexedListChildPath<ExprElement>({ expressions }, getElementPathHash());
	}
}
