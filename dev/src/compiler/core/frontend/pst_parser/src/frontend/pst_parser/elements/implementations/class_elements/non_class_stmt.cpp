#include "../../hierarchy/class_elements/non_class_stmt.hpp"

#include "preamble.hpp"

namespace pst {
	void NonClassStmt::cloneSubElements(const NonClassStmt& other) {
		ELEMENT_CLONE_SUB_ELEMENT(inner_stmt);
		inner_decl_symbol_name = inner_stmt.internal()->getDeclSymbolIdentifier();
		ParentClass::cloneSubElements(other);
	}

	MBox<NonClassStmt> NonClassStmt::parse(LangParserState& state) {
		auto out = makeBox<NonClassStmt>(state);

		Keyword as_keyword = state[0].asKeyword();
		CORE_ASSERT(
			as_keyword == Keyword::Alias || as_keyword == Keyword::Using
				|| as_keyword == Keyword::Class,
			"Bad starting keyword in NonClassStmt."
		);

		PARSE().one(&out->inner_stmt);
		out->inner_decl_kind        = out->inner_stmt.internal()->isDeclaration();
		out->inner_decl_symbol_name = out->inner_stmt.internal()->getDeclSymbolIdentifier();

		PST_RETURN out;
	}

	void NonClassStmt::dprint(std::ostream& out) const {
		out << "{";

		out << R"("internal stmt": )";
		nullAwareDprint(inner_stmt, out);

		out << "}";
	}

	HashAlg& NonClassStmt::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, inner_decl_kind);
		addToHash(partial_hash, inner_decl_symbol_name.has_value());
		return partial_hash;
	}

	void NonClassStmt::acceptVisitor(PstVisitor& visitor) const {
		inner_stmt.internal()->acceptVisitor(visitor);
	}
}
