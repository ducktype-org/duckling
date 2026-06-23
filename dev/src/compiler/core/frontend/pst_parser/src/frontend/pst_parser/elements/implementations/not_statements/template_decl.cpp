#include "../../hierarchy/not_statements/template_decl.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(TemplateDecl, params);

	MBox<TemplateDecl> TemplateDecl::parse(LangParserState& state) {
		auto out = makeBox<TemplateDecl>(state);

		if (!assertStmtChoice<TemplateDecl>(state, state[0].is(Keyword::Template))) return nullptr;

		PARSE().one(Keyword::Template);
		if (state[0].isBracketGroup(lexer::Token::Round)) {
			state.logSafeError(
				makeBox<TemplateNoListError>(state.getPosition())
			);
		}
		PARSE().one(&out->params);

		PST_RETURN out;
	}

	void TemplateDecl::dprint(std::ostream& out) const {
		out << "{";

		out << ",\"Parameters\":";
		nullAwareDprint(params, out);

		out << "}";
	}


	HashAlg& TemplateDecl::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
