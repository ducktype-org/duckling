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
		AccessInternal<Stmt> inner_stmt;

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

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
