#include "../../hierarchy/expressions/comma.hpp"

#include "../../hierarchy/expressions/match_expr.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> Comma::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Comma" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		std::vector<i64> ends;
		for (i64 i = 0; i < length; i++)
			if (state[i].is(Special::Comma)) ends.push_back(i);
		if (ends.empty()) return Lower::parse(state, length);
		auto out   = makeBox<Comma>(pos);
		i64  start = -1;

		for (auto end: ends) {
			out->expressions.emplace_back();
			state.parse(out).with(&out->expressions.back(), Lower::parse, end - 1 - start);
			state.parse(out).one(Special::Comma);
			start = end;
		}
		if (start + 1 != length) {
			out->expressions.emplace_back();
			state.parse(out).with(&out->expressions.back(), Lower::parse, length - 1 - start);
		}
		return out;
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

	LangElement::HashAlg& Comma::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, expressions.size());
		return partial_hash;
	}

	void Comma::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitComma(*this); }

	void Comma::calcComponentHashRecursive() {
		calcIndexedListChildPath<ExprElement>({ expressions }, getComponentHash());
	}
}
