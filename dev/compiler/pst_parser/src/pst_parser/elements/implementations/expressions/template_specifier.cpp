#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> TemplateSpecifier::parse(LangParserState& state, u64 length) {
		// std::cerr << "Parsing Template Specifier" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (length != 2) {}  // Error

		auto out = base::make_unique<TemplateSpecifier>(state.getPosition());
		state.parse(out).one(NamedOperator::Colon);
		state.parse(out).goDown();
		state.parse(out).with(&out->inner, Comma::parse, state.ctokens().size());
		state.parse(out).goUpAndSkip();

		return out;
	}

	void TemplateSpecifier::dprint(std::ostream& out) const {
		out << "{";

		out << R"("inner": )";
		nullAwareDprint(inner, out);

		out << "}";
	}

	void TemplateSpecifier::acceptVisitor(PstExprVisitor& visitor) const {
		visitor.visitTemplateSpecifier(*this);
	}
}
