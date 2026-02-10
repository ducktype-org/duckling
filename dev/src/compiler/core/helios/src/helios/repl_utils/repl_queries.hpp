#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/hout.hpp>

#include <query_framework/query_int.hpp>

#include <vm/core/process/interface_types.hpp>

namespace compiler::repl {

	/**
	 * @brief Key for QueryReplExpressionWrapper
	 */
	struct QueryReplExpressionWrapper_Key {
		pst::AccessLocked<pst::ExprStmt> expr_stmt;
		u64                              counter;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Query to build a HOUT function that wraps a REPL expression
	 *
	 * Takes an ExprStmt directly and builds a complete HOUTFunction with the
	 * expression wrapped in a return statement.
	 */
	DECLARE_QUERY(
		QueryReplExpressionWrapper,
		QueryReplExpressionWrapper_Key,
		helios::HOUTFunction,
		({ .uses_qresult = false })
	);

}  // namespace compiler::repl
