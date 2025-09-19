#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Allows for limited non-class statements to be in a class.
	 *
	 * Currently allows: using, alias.
	 */
	class NonClassStmt: public ClassStmt {
		NAMED_CHILD(inner_stmt, Stmt);

		HashAlg& calcStableHash(HashAlg& partial_hash) const override;

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
			return DeclKind::Transparent;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
