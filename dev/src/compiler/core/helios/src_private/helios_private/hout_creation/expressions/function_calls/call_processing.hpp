// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/hout.hpp>

#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given call expression and creates CallExpr from it. The function is selected based on
	 * argument types and named arguments, but not the name, so all provided candidates must have
	 * the expected name. If no function or multiple functions match the call, an error is returned.
	 *
	 * @note takes actual symbols that might be called, does not perform any lookup.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param callee_element The PST element representing the identifier of the function being
	 * called, like `foo` in `foo(10)`.
	 * @param call_expr The PST call expression representing the function call. (the `(...)` part
	 * and not the callee)
	 */
	query::QResult<Box<Expr>> processFunctionCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr
	);

	/**
	 * @brief Same as above, but for methods. The "self" argument is passed as the first argument,
	 * and it is implicitly taken by `ref` when the selected overload expects a `ref`.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param callee_element The PST element representing the identifier of the function being
	 * called, like `foo` in `object.foo(10)`.
	 * @param call_expr The PST call expression representing the function call. (the `(...)` part
	 * and not the callee)
	 * @param self_arg The processed "self" argument of the method call, with its original value
	 * category.
	 */
	query::QResult<Box<Expr>> processMethodCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr,
		Box<Expr>                     self_arg
	);

	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given argument expressions and creates a callexpr from it. The function is selected based
	 * on argument types only, not the name, so all provided candidates must have the expected name.
	 * If no function or multiple functions match the call, an error is returned.
	 *
	 * @note takes actual symbols that might be called, does not perform any lookup.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param method_candidates The subset of candidates which are methods, so the left-hand side
	 * is their `self` argument and may be implicitly taken by `ref`.
	 * @param lhs The preprocessed left-hand side argument of the operator call.
	 * @param rhs The preprocessed right-hand side argument of the operator call.
	 * @param op_origin Operator origin used for callee origin.
	 */
	query::QResult<Box<Expr>> processBinaryOperatorCall(
		query::Context&           ctx,
		const std::vector<SymID>& candidates,
		const std::vector<SymID>& method_candidates,
		Box<Expr>                 lhs,
		Box<Expr>                 rhs,
		ElementOrigin             op_origin
	);

	/**
	 * @brief Determines the correct function to call (i.e. performs the overload resolution) from
	 * the given argument expression and creates a callexpr from it. The function is selected based
	 * on the argument type only, not the name, so all provided candidates must have the expected
	 * name. If no function or multiple functions match the call, an error is returned.
	 *
	 * @note takes actual symbols that might be called, does not perform any lookup.
	 *
	 * @param candidates Contains all candidate functions that could be called.
	 * @param method_candidates The subset of candidates which are methods, so the operand is their
	 * `self` argument and may be implicitly taken by `ref`.
	 * @param inner The preprocessed argument of the operator call.
	 * @param op_origin Operator origin used for callee origin.
	 * @param operatoriness Whether we're dealing with a prefix or suffix operator.
	 */
	query::QResult<Box<Expr>> processUnaryOperatorCall(
		query::Context&                        ctx,
		const std::vector<SymID>&              candidates,
		const std::vector<SymID>&              method_candidates,
		Box<Expr>                              inner,
		ElementOrigin                          op_origin,
		HOUTFunctionDeclaration::Operatoriness operatoriness
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
}
