#include "../../hierarchy/expressions/template_specifier.hpp"

#include "../../hierarchy/lists/template_list.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(TemplateSpecifier, inner);

	MBox<ExprElement> TemplateSpecifier::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		if (length != 2) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadTemplateError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<TemplateSpecifier>(state);
		PARSE().one(NamedOperator::Colon);
		PARSE().goDown();
		PARSE().one(&out->inner);
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void TemplateSpecifier::dprint(std::ostream& out) const {
		out << "{";

		out << R"("inner": )";
		nullAwareDprint(inner, out);

		out << "}";
	}

	HashAlg& TemplateSpecifier::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void TemplateSpecifier::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitTemplateSpecifier(*this);
	}
}
