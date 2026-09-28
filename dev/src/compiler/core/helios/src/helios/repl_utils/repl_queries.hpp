#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	struct ReplInputWrapper;
}

namespace compiler::repl {
	/**
	 * @brief Build the HOUT function wrapping a single REPL/script input statement.
	 *
	 * The wrapped PST element is recovered from `input.pst_element_hash`, and `input.type` decides
	 * how it is turned into the function body:
	 * - `Expression`: a single return statement of the expression (an expression statement, if the
	 *   expression is of type void),
	 * - `Instruction`: the statement compiled into a unit-returning body.
	 * - `GlobalInitializer`: the constructor expression of the declared global variable, in a
	 *   unit-returning body.
	 *
	 * @note This is the implementation of QueryCodeOfFun for `ReplInputWrapper` symbols, it is not
	 * meant to be called directly.
	 */
	helios::HOUTFunction getReplInputFunction(
		query::Context& ctx, const helios::defgen::ReplInputWrapper& input
	);


	/**
	 * @brief Helpr to get the SymID of a global variable given the PST element.
	 */
	helios::SymID getVariableSymID(query::Context& ctx, pst::AccessLocked<pst::Variable> var_stmt);

	// ========================== Helios Symbols Factories ==========================

	/**
	 * @brief Get the symbol standing for the wrapper function of the instruction.
	 */
	helios::SymID queryReplInstructionWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, u64 counter
	);

	/**
	 * @brief Get the symbol standing for the wrapper function of the expression.
	 */
	query::QResult<helios::SymID> queryReplExpressionWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::ExprStmt> expr_stmt
	);

	/**
	 * @brief Get the symbol standing for the wrapper function of the variable initializer expression.
	 */
	helios::SymID queryReplGlobalInitializerWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::Variable> var_stmt
	);

	/**
	 * @brief Get the symbol standing for the storage of a REPL/script global variable, holding an
	 * empty (zero) value instead of the declared initial one.
	 */
	helios::SymID queryReplEmptyVariableSymbol(query::Context& ctx, helios::SymID variable_symbol);

	// @TODO docs
	helios::SymID queryHoutExpressionWrapperSymbol(
		query::Context& ctx, base::StrID sym_name, Box<helios::code::Expr> expr
	);

}  // namespace compiler::repl
