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
		auto position = state.getPosition();
		auto out      = makeBox<For>(position);

		if (!assertStmtChoice<For>(state, state[0].is(Keyword::For))) return nullptr;

		state.parse(out).all(Keyword::For, &out->optional_name);

		if (!state[0].isBracketGroup(Token::Round)) {
			state.logInt(makeBox<ForBracketError>(state.getPosition()));
		} else {
			state.parse(out).goDown();

			state.parse(out).one(&out->iterator, true);

			if (state.parse(out).tryEat(NamedOperator::Colon)) {
				state.parse(out).one(&out->type);
				state.parse(out).tryEat(Keyword::In);
			} else {
				state.parse(out).one(Keyword::In);
			}

			state.parse(out).one(&out->iterable);

			state.parse(out).goUpAndSkip();
		}

		state.parse(out).withDef(&out->body, CodeBlock::CodeBlockType::Ordered);

		PST_RETURN out;
	}

	void For::dprint(std::ostream& out) const {
		out << "{";
		out << R"("name":)";
		nullAwareDprint(optional_name, out);
		out << R"(, "identifier": )";
		nullAwareDprint(iterator, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << R"(, "iterable": )";
		nullAwareDprint(iterable, out);
		out << R"(, "body": )";
		nullAwareDprint(body, out);
		out << "}";
	}

	LangElement::HashAlg& For::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, optional_name);
		addToHash(partial_hash, iterator);
		return partial_hash;
	}

	void For::acceptVisitor(PstVisitor& visitor) const { visitor.visitFor(*this); }
}
