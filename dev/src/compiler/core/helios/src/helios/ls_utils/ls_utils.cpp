#include "ls_utils.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include "frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp"
#include <frontend/pst_parser/elements/hierarchy/not_statements/attribute.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/dotted_name.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/stmt_specifier.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include "helios_private/expressions/query_hout_of_expr.hpp"

namespace compiler::helios::ls {

	CRef<query::QResult<Box<code::Expr>>> getHoutExpr(
		query::Context& ctx, pst::Access<pst::ExprElement> element
	) {
		return ctx.query<QueryHoutOfExpr>({element});
	}
}
