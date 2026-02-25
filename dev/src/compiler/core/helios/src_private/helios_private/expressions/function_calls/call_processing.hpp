#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <helios/hout/elements/expr.hpp>

#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {

	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given call expression and creates CallExpr from it. The function is selected based on
	 * argument types and named arguments. If no function or multiple functions match the call, an
	 * error is returned.
	 *
	 * @note takes actual symbols that might be called, does not perform any lookup.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param callable_expr The PST expression representing the callee being invoked.
	 * @param callee_element The PST element representing the identifier of the function being
	 * called, like `foo` in `foo(10)`.
	 * @param call_expr The PST call expression representing the function call. (the `(...)` part
	 * and not the callee)
	 */
	query::QResult<Box<CallExpr>> processFunctionCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr
	);


	/**
	 * @brief Same as above, but for methods. It finds the "self" argument and then calls the same
	 * overload resolution as for normal function calls.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param callable_expr The PST expression representing the callee being invoked.
	 * @param callee_element The PST element representing the identifier of the function being
	 * called, like `foo` in `object.foo(10)`.
	 * @param call_expr The PST call expression representing the function call. (the `(...)` part
	 * and not the callee)
	 */
	query::QResult<Box<CallExpr>> processMethodCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr,
		Box<Expr>                     self_arg
	);

	/**
	 * @brief The HELIOS arguments of the call, both positional and named,
	 * used for the later processing by `callOverloadResolution` function.
	 */
	struct CallArguments {
		std::vector<Box<Expr>>                          positional_arguments;
		std::vector<std::tuple<base::StrID, Box<Expr>>> named_arguments;
	};

	/**
	 * @brief The PST origins of the call, used for constructing the proper origin of the CallExpr
	 * and for error reporting. Used only by the `callOverloadResolution` function.
	 */
	struct CallPstOrigin {
		/// The PST origin of the whole call for example `namespace.object.method(arg1, arg2)`
		ElementOrigin whole_call_origin;
		// The `identifier` part of the call, for example only the `method` part of the previous example
		ElementOrigin callee_origin;
		// note. fields above could be a PST only structure in the future, since the ElementOrigin
		// can be also generated, we loose some information and have to assume that there are PST
		// elements inside.

		/// The ElementOrigin of each argument, not that some arguments may be generated like `self`
		/// argument.
		std::vector<ElementOrigin>
			arguments_origin;  // whole arguments, with the name if it is a named argument, for
		                       // example `arg1`, `arg2` or `name: arg3`
	};

	/**
	 * @brief More general version that performs the matching of the provided arguments,
	 * logs the errors and constructs the CallExpr.
	 *
	 * @param ctx
	 * @param candidates Candidates for the call.
	 * @param call_arguments The HOUT expressions of the arguments of the call, both positional and
	 * named.
	 * @param pst_origin The PST origin of the whole call, callee and arguments, used for error
	 * reporting.
	 * @return query::QResult<Box<CallExpr>>
	 */
	query::QResult<Box<CallExpr>> callOverloadResolution(
		query::Context&           ctx,
		const std::vector<SymID>& candidates,
		CallArguments             call_arguments,
		const CallPstOrigin&      pst_origin
	);
}
