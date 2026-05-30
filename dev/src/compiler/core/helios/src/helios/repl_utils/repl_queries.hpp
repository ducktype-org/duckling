#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/hout.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

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
		query::QResult<helios::HOUTFunction>,
		({})
	);

	/**
	 * @brief Key for QueryReplInstructionWrapper
	 */
	struct QueryReplInstructionWrapper_Key {
		pst::AccessLocked<pst::Stmt> stmt;
		u64                          counter;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Query to build a HOUT function that wraps a REPL instruction.
	 *
	 * Takes a single instruction statement (if/while/for/block) and builds a complete
	 * HOUTFunction that executes the instruction inside a synthetic void function body.
	 * This allows the DVM to execute bare instructions via runFunction.
	 */
	DECLARE_QUERY(
		QueryReplInstructionWrapper,
		QueryReplInstructionWrapper_Key,
		query::QResult<helios::HOUTFunction>,
		({})
	);

}  // namespace compiler::repl
