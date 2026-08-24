#include "../../hierarchy/expressions/format_string_value.hpp"

#include "../../hierarchy/not_statements/format_string_sub_elements/format_sub_expression.hpp"
#include "../../hierarchy/not_statements/format_string_sub_elements/format_sub_string.hpp"
#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(ExprFormatStrValue, sub_elements);

	MBox<ExprElement> ExprFormatStrValue::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		CORE_ASSERT(state[0].is(Token::Type::FormatString), "Bad choice of atom.");

		auto out = base::makeBox<ExprFormatStrValue>(state);

		PARSE().goDown();

		PST_WHILE(state.ctokens().size() > 0) {
			if (state[0].is(Token::Type::BracketGroup)) {
				MBox<FormatSubExpression> sub_expr;
				PARSE().one(&sub_expr);
				if (!sub_expr) return nullptr;
				out->sub_elements.emplace_back(nullptr);
				PARSE().assign(&out->sub_elements.back(), std::move(sub_expr));
			} else if (state[0].is(Token::Type::FormatStringSubString)) {
				MBox<FormatSubString> sub_str;
				PARSE().one(&sub_str);
				if (!sub_str) return nullptr;
				out->sub_elements.emplace_back(nullptr);
				PARSE().assign(&out->sub_elements.back(), std::move(sub_str));
			} else {
				CORE_PANIC("Bad format string lexing");
			}
		}

		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void ExprFormatStrValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("sub-elements": [)";
		bool first = true;
		for (auto& el: sub_elements) {
			if (!first)
				out << ", ";
			else
				first = false;
			nullAwareDprint(el, out);
		}
		out << "]";

		out << "}";
	}

	HashAlg& ExprFormatStrValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, sub_elements.size());
		return partial_hash;
	}

	void ExprFormatStrValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprFormatStrValue(*this);
	}

	void ExprFormatStrValue::calcElementPathHashRecursive() {
		calcIndexedListChildPath<FormatSubElement>({ sub_elements }, getElementPathHash());
	}
}
