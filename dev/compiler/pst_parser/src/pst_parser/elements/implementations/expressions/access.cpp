#include "preamble.hpp"

namespace pst::expr {
	class BadAccessError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected single access expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadAccessError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> Access::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Access Specifier" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (length != 2 && length != 4) {
			state.log(base::make_unique<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<Access>(state.getPosition());

		out->type = state[0].getValue();
		state.parse(out).eatOne();  // `.` or `.?`
		state.parse(out).one(&out->name);

		if (length > 2 && state[0].is(NamedOperator::Colon)
		    && state[1].isBracketGroup(Token::Curly))
			state.parse(out).with(&out->template_specifier, TemplateSpecifier::parse, 2L);

		return out;
	}

	void Access::dprint(std::ostream& out) const {
		out << "{";

		out << R"("type": ")" << type.str() << "\"";
		out << R"(, "name": )";
		nullAwareDprint(name, out);
		if (template_specifier) {
			out << R"(, "template specifier": )";
			nullAwareDprint(template_specifier.value(), out);
		}

		out << "}";
	}

	void Access::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitAccess(*this); }
}
