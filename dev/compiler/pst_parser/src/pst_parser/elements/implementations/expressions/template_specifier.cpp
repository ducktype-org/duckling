#include "preamble.hpp"

namespace pst::expr {
	class BadTemplateError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected single template instantiation expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadTemplateError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> TemplateSpecifier::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (length != 2) {
			// This should (probably) never happen with how it's called by the parser
			state.log(base::make_unique<BadTemplateError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<TemplateSpecifier>(state.getPosition());
		state.parse(out).one(NamedOperator::Colon);
		state.parse(out).goDown();
		state.parse(out).one(&out->inner);
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
