#include "../../hierarchy/declarations/for.hpp"

#include "../../hierarchy/expr_holders.hpp"                            // IWYU pragma: keep
#include "../../hierarchy/expressions/comma.hpp"
#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {

	bool ExprParserHelper::untilForTypeEnd(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Semicolon) || state[fwd].is(NamedOperator::Assign)
		    || state[fwd].is(Keyword::In);
	}

	MBox<For> For::parse(LangParserState& state) {
		auto out = makeBox<For>(state);

		if (!assertStmtChoice<For>(state, state[0].is(Keyword::For))) return nullptr;

		PARSE().all(Keyword::For, &out->name);

		if (!state[0].isBracketGroup(Token::Round)) {
			state.logInt(makeBox<ForBracketError>(state.getPosition()));
		} else {
			PARSE().goDown();

			PARSE().one(&out->iterator);

			if (PARSE().tryEat(NamedOperator::Colon)) {
				PARSE().one(&out->type);
				PARSE().tryEat(Keyword::In);
			} else {
				PARSE().one(Keyword::In);
			}

			PARSE().one(&out->iterable);

			PARSE().goUpAndSkip();
		}

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().one(&out->body);
		})

		PST_RETURN out;
	}

	void For::dprint(std::ostream& out) const {
		out << "{";
		if (name.has_value()) {
			out << R"("name":)";
			nullAwareDprint(name.value(), out);
			out << ",";
		}
		out << R"("identifier": )";
		nullAwareDprint(iterator, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << R"(, "iterable": )";
		nullAwareDprint(iterable, out);
		out << R"(, "body": )";
		nullAwareDprint(body, out);
		out << "}";
	}

	HashAlg& For::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void For::acceptVisitor(PstVisitor& visitor) const { visitor.visitFor(*this); }
}
