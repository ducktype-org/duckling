#include "../../hierarchy/expressions/access.hpp"

#include "../../hierarchy/expressions/template_specifier.hpp"
#include "preamble.hpp"

#include "expressions_errors.hpp"

namespace pst::expr {

	MBox<ExprElement> Access::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (length != 2 && length != 4) {
			state.logInt(makeBox<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<Access>(state.getPosition());

		out->type = state[0].getValue();

		// This should never occur if access parsing is called well
		if (!state[0].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
		    ).copyValueOr(false)) {
			state.logInt(makeBox<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		state.parse(out).eatOne();  // `.` or `.?`
		state.parse(out).one(&out->name);

		if (length > 2 && state[0].is(NamedOperator::Colon) && state[1].isBracketGroup(Token::Curly))
			state.parse(out).with(&out->template_specifier, TemplateSpecifier::parse, 2L);

		return out;
	}

	void Access::dprint(std::ostream& out) const {
		out << "{";

		out << R"("type": ")" << type.strView() << "\"";
		out << R"(, "name": )";
		nullAwareDprint(name, out);
		if (template_specifier) {
			out << R"(, "template specifier": )";
			nullAwareDprint(template_specifier.value(), out);
		}

		out << "}";
	}

	LangElement::HashAlg& Access::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, type.strView());
		addToHash(partial_hash, name);
		addToHash(partial_hash, template_specifier.has_value());
		return partial_hash;
	}

	void Access::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitAccess(*this); }

	base::StrID Access::getType() const { return type; }

	const tpc::Identifier& Access::getName() const { return name; }
}
