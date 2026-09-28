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
	 * @brief Build the HOUT function wrapping a single REPL/script input or a HOUT expression.
	 *
	 * `input.element` decides how the function body is built:
	 * - `Expression`: a single return statement of the stored HOUT expression, moved from if its
	 *   value category allows it,
	 * - `Instruction`: the PST statement recovered from its stable hash, compiled into a
	 *   unit-returning body.
	 *
	 * @param sym The symbol of the wrapper function, whose declaration is used for the result.
	 * @note This is the implementation of QueryCodeOfFun for `ReplInputWrapper` symbols, it is not
	 * meant to be called directly.
	 */
	helios::HOUTFunction getReplInputFunction(
		query::Context& ctx, helios::SymID sym, const helios::defgen::ReplInputWrapper& input
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
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt
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

	/**
	 * @brief Get the symbol standing for a function wrapper that returns the given HOUT
	 * expression.
	 * @param sym_name Name of the generated symbol.
	 * @param expr The expression to be returned by the wrapper, the wrapper takes its ownership.
	 */
	helios::SymID queryHoutExpressionWrapperSymbol(
		query::Context& ctx, base::StrID sym_name, Box<helios::code::Expr> expr
	);

}  // namespace compiler::repl
