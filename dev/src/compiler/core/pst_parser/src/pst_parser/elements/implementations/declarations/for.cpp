#include "../../hierarchy/declarations/for.hpp"

#include "../../hierarchy/expr_holders.hpp"                            // IWYU pragma: keep
#include "../../hierarchy/expressions/comma.hpp"
#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	class ForBracketError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected round bracket group.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		ForBracketError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	namespace {
		bool isForTypeEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon) || state[fwd].is(NamedOperator::Assign)
			    || state[fwd].is(Keyword::In);
		}
	}

	MBox<ExprElement> ExprParserHelper::parseForType(LangParserState& state) {
		return expr::parseUntil<expr::Comma, isForTypeEnd>(state);
	}

	MBox<For> For::parse(LangParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeBox<For>(position);

		if (!assertStmtChoice<For>(state, state[0].is(Keyword::For))) return nullptr;

		state.parse(out).all(Keyword::For, &out->optional_name);

		if (!state[0].isBracketGroup(Token::Round)) {
			state.log(makeBox<ForBracketError>(state.getPosition()));
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

		state.parse(out).one(&out->body, true);

		return out;
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

	void For::acceptVisitor(PstVisitor& visitor) const { visitor.visitFor(*this); }
}
