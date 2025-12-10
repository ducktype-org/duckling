#include "../../hierarchy/not_statements/expr_element.hpp"

#include "../../hierarchy/expressions/assignment.hpp"
#include "../../hierarchy/expressions/comma.hpp"
#include "../../hierarchy/expressions/match_expr.hpp"
#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	class EmptyExprError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Empty expression where non-empty expected";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyExprError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	void ExprElement::fastForward(LangParserState& state, i64 length) {
		state.tokens().skip(length);
	}

	bool ExprElement::checkLength(LangParserState& state, i64 length) {
		if (length <= 0) {
			// Empty expression error
			state.log(makeBox<EmptyExprError>(state.getPosition()));
			fastForward(state, length);
			return false;
		}
		if (state[length - 1].is(lexer::Token::Type::Sentinel))
			CORE_PANIC("Internal error too long expression\n");
		return true;
	}

	void ExprHolder::dprint(std::ostream& out) const {
		out << "{";

		out << R"("expr":)";
		nullAwareDprint(expr, out);

		out << "}";
	}

	LangElement::HashAlg& ExprHolder::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	namespace {
		bool universalEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Comma) || state[fwd].is(Special::Semicolon)
			    || ExprClassify::isAssignment(state, fwd)
			    || internal::Conditions::isBlockGroup(state, fwd);
		}

		bool universalAllowBlockEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Comma) || state[fwd].is(Special::Semicolon)
			    || ExprClassify::isAssignment(state, fwd);
		}

		bool universalEndAllowComma(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon) || ExprClassify::isAssignment(state, fwd)
			    || internal::Conditions::isBlockGroup(state, fwd);
		}

		bool assignmentEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon);
		}
	}

	MBox<ExprElement> ExprParserHelper::parseUniversal(LangParserState& state) {
		return expr::parseUntil<expr::MatchExpr, universalEnd>(state);
	}

	MBox<ExprElement> ExprParserHelper::parseUniversalAllowBlock(LangParserState& state) {
		return expr::parseUntil<expr::MatchExpr, universalAllowBlockEnd>(state);
	}

	MBox<ExprElement> ExprParserHelper::parseComma(LangParserState& state) {
		return expr::parseUntil<expr::Comma, universalEndAllowComma>(state);
	}

	MBox<ExprElement> ExprParserHelper::parseAssignment(LangParserState& state) {
		return expr::parseUntil<expr::Assignment, assignmentEnd>(state);
	}
}
