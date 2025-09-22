/**
 * @file mir_lowering.cpp
 * @brief Implementation of lowering HOUT functions to MIR functions.
 * The creation of MIR is done "in reverse" that is from function end to its beginning.
 */

#include "mir_lowering.hpp"

#include "mir_builders.hpp"
#include <mir/mir_lowering/mir_lifetimes.hpp>
#include <mir/mir_lowering/mir_validation.hpp>

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/exceptions.hpp>
#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>
#include <base/variant.hpp>

#include <query_framework/query_impl.hpp>
#include <query_framework/query_result.hpp>

#include <ranges>
#include <stack>
#include <unordered_set>
#include <variant>

namespace compiler::mir {
	namespace hc = helios::code;

	StmtLowerRes lowerStmt(
		const hc::Stmt&  stmt,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         parent_scope
	) {
		StmtBlockVisitor visitor{ continuation, function, parent_scope };
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes lowerExpr(
		const hc::Expr&  expr,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         expr_scope
	) {
		ExprBlockVisitor visitor{ continuation, function, expr_scope };
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block,
		BlockBuilderRef      continuation,
		FunctionBuilder&     function,
		ScopeRef             parent_scope
	) {
		StmtLowerRes last_result{ continuation };
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result  = lowerStmt(*stmt, continuation, function, parent_scope);
			continuation = last_result.begin;
		}
		return last_result;
	}

	// @TODO: StmtExprBoolJmpVisitor for jumping code
}
