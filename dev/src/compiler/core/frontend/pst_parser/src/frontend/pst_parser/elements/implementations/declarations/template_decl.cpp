#include "../../hierarchy/declarations/template_decl.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(TemplateDecl, params, inner_statement);

	MBox<TemplateDecl> TemplateDecl::parse(LangParserState& state) {
		auto out = makeBox<TemplateDecl>(state);

		if (!assertStmtChoice<TemplateDecl>(state, state[0].is(Keyword::Template))) return nullptr;

		PARSE().all(Keyword::Template);
		PARSE().one(&out->params);
		PARSE().one(&out->inner_statement);

		PST_RETURN out;
	}

	void TemplateDecl::dprint(std::ostream& out) const {
		out << "{";
		out << "\"parameters\":";
		nullAwareDprint(params, out);
		out << ",\"inner_statement\":";
		nullAwareDprint(inner_statement, out);
		out << "}";
	}


	HashAlg& TemplateDecl::addElementDataToStableHash(HashAlg& partial_hash) const {
		// add name here?, or not
		return partial_hash;
	}

	void TemplateDecl::acceptVisitor(PstVisitor& visitor) const { visitor.visitTemplateDecl(*this); }
}
