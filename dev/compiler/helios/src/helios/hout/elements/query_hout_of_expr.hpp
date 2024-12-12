#include "expr.hpp"

namespace compiler::helios {
	struct KeyOf_QueryHoutOfExpr {
		MCRef<pst::ExprElement> expr;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope
	 */
	DECLARE_QUERY(QueryHoutOfExpr, KeyOf_QueryHoutOfExpr, errors::HResult<base::Box<code::Expr> COMMA errors::Failed>)
}
