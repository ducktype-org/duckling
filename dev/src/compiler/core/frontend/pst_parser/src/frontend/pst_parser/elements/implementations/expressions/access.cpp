#include "../../hierarchy/expressions/access.hpp"

#include "../../hierarchy/expressions/template_specifier.hpp"
#include "expressions_errors.hpp"
#include "preamble.hpp"

namespace pst::expr {

	CLONE_SUB_ELEMENTS_DEF(Access, type, name, template_specifier);

	MBox<ExprElement> Access::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		if (length != 2 && length != 4) {
			state.logInt(makeBox<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<Access>(state);

		// This should never occur if access parsing is called well
		if (!state[0].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
		    ).copyValueOr(false)) {
			state.logInt(makeBox<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		PARSE().all(&out->type, &out->name);

		if (state.ctokens().size() >= 2 && state[0].is(NamedOperator::Colon)
		    && state[1].isBracketGroup(Token::Curly))
			PARSE().with(&out->template_specifier, TemplateSpecifier::parse);

		PST_RETURN out;
	}

	void Access::dprint(std::ostream& out) const {
		out << "{";

		out << R"("type": )";
		nullAwareDprint(type, out);
		out << R"(, "name": )";
		nullAwareDprint(name, out);
		if (template_specifier) {
			out << R"(, "template specifier": )";
			nullAwareDprint(template_specifier.value(), out);
		}

		out << "}";
	}

	HashAlg& Access::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, template_specifier.has_value());
		return partial_hash;
	}

	void Access::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitAccess(*this); }
}
