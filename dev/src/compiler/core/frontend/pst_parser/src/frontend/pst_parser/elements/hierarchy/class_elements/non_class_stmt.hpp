#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

#include "../not_statements/wrapper_elements/identifier_wrapper.hpp"

namespace pst {
	/**
	 * @brief Allows for limited non-class statements to be in a class.
	 *
	 * @note It currently passes the DeclKind and symbol name from the inner statement because it
	 * should help lookup behaviour.
	 *
	 * Currently allows: using, alias, class.
	 */
	class NonClassStmt: public ClassStmt {
		NAMED_CHILD(inner_stmt, Stmt);

		DeclKind                    inner_decl_kind = DeclKind::None;
		base::Optional<AccessLocked<IdentifierWrapper>> inner_decl_symbol_name;

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(NonClassStmt, ElementKind::NonClassStmt);
		CLASS_STMT_PARSE(NonClassStmt);

		~NonClassStmt() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Non Class Statement";
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return false;
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return inner_decl_kind;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbol2() const override {
			return inner_decl_symbol_name;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
