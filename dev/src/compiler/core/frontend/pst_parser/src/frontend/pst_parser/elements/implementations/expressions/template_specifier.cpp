#include "../../hierarchy/expressions/template_specifier.hpp"

#include "../../hierarchy/lists/template_list.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst::expr {
	class BadTemplateError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_template_error" };
		}

	public:
		BadTemplateError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<ExprElement> TemplateSpecifier::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (length != 2) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadTemplateError>(
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

	LangElement::HashAlg& TemplateSpecifier::addElementDataToStableHash(HashAlg& partial_hash
	) const {
		return partial_hash;
	}

	void TemplateSpecifier::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitTemplateSpecifier(*this);
	}
}
