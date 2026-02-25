#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <helios/hout/elements/expr.hpp>

#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {

	query::QResult<Box<CallExpr>> processFunctionCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr
	);

	query::QResult<Box<CallExpr>> processMethodCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr,
		Box<Expr>                     self_arg
	);

	struct CallArguments {
		std::vector<Box<Expr>>                          positional_arguments;
		std::vector<std::tuple<base::StrID, Box<Expr>>> named_arguments;
	};

	struct CallPstOrigin {
		ElementOrigin whole_call_origin;  // for example `namespace.object.method(arg1, arg2)`
		ElementOrigin callee_origin;      // only the `method` part of the previous example
		std::vector<ElementOrigin>
			arguments_origin;  // whole arguments, with the name if it is a named argument, for
		                       // example `arg1`, `arg2` or `name: arg3`
	};

	query::QResult<Box<CallExpr>> callOverloadResolution(
		query::Context&           ctx,
		const std::vector<SymID>& candidates,
		CallArguments             call_arguments,
		const CallPstOrigin&      pst_origin
	);
}
