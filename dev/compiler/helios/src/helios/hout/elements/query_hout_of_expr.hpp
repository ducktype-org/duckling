#include "expr.hpp"

namespace compiler::helios {
	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope
	 */
	DECLARE_QUERY(QueryHoutOfExpr, pst::GenericPSTQueryKey<pst::ExprElement>, errors::HResult<base::Box<code::Expr> COMMA errors::Failed>)
}
