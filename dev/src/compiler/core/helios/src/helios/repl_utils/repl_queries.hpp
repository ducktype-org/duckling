#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
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

	/**
	 * @brief Key for QueryReplVariableWrapper
	 */
	struct QueryReplVariableWrapper_Key {
		pst::AccessLocked<pst::Variable> var_stmt;
		u64                              counter;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Result of QueryReplVariableWrapper: the storage of a REPL global variable and the
	 * function that initializes it.
	 */
	struct QueryReplVariableWrapper_Result final {
		/**
		 * @brief Global data for the variable, always holding an empty (zero) initializer, so that
		 * no initialization happens at global data creation time.
		 */
		helios::HOUTGlobalData global_data;

		/**
		 * @brief Synthetic void function constructing the variable from its declared initial value.
		 */
		helios::HOUTFunction initializer_function;
	};

	/**
	 * @brief Query to build the global data and the initializing HOUT function for a REPL variable
	 * declaration.
	 *
	 * Takes a variable declaration statement and splits it into two parts:
	 * - global data whose initial value is an empty (zero) value for every type, so that declaring
	 *   the variable never runs any initialization on its own,
	 * - a synthetic void function that performs the actual construction from the declared initial
	 *   value, which the DVM can execute via runFunction.
	 */
	DECLARE_QUERY(
		QueryReplVariableWrapper,
		QueryReplVariableWrapper_Key,
		CRef<query::QResult<QueryReplVariableWrapper_Result>>,
		({})
	);

}  // namespace compiler::repl
