#include "../../hierarchy/declarations/template_stmt.hpp"

#include "preamble.hpp"

namespace pst {
	void TemplateStmt::cloneSubElements(const TemplateStmt& other) {
		ELEMENT_CLONE_SUB_ELEMENT(template_decl);
		ELEMENT_CLONE_SUB_ELEMENT(inner_statement);
		inner_decl_symbol = inner_statement.internal()->getDeclSymbolIdentifier();
<<<<<<< HEAD
    	ParentClass::cloneSubElements(other);
=======
		ParentClass::cloneSubElements(other);
>>>>>>> origin/main
	}

	MBox<TemplateStmt> TemplateStmt::parse(LangParserState& state) {
		auto out = makeBox<TemplateStmt>(state);

		if (!assertStmtChoice<TemplateStmt>(state, state[0].is(Keyword::Template))) return nullptr;

		PARSE().one(&out->template_decl);
		PARSE().one(&out->inner_statement);

		PST_RETURN out;
	}

	void TemplateStmt::dprint(std::ostream& out) const {
		out << "{";

		out << "\"template_decl\":";
		nullAwareDprint(template_decl, out);
		out << ",\"inner_statement\":";
		nullAwareDprint(inner_statement, out);

		out << "}";
	}

<<<<<<< HEAD

=======
>>>>>>> origin/main
	HashAlg& TemplateStmt::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, inner_decl_symbol.has_value());
		return partial_hash;
	}

<<<<<<< HEAD
	void TemplateStmt::acceptVisitor(PstVisitor& visitor) const { visitor.visitTemplateStmt(*this); }
=======
	void TemplateStmt::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitTemplateStmt(*this);
	}
>>>>>>> origin/main
}
