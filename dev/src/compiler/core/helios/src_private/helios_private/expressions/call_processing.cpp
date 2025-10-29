#include "call_processing.hpp"

#include "coercions.hpp"

#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <typesystem/higher/types.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context.hpp>

#include <vector>

namespace compiler::helios::code {

	/**
	 * Checks if a function can be called with the given arguments for overload resolution
	 * based on function declaration.
	 * It has duplicate code with the `attemptFittingFun`, but this function never invalidates the
	 * arguments.
	 * @TODO: #1362 After changing QueryHoutOfExpr to return Ref instead of Box it will be possible
	 * to deduplicate this function with `attemptFittingFun`.
	 */
	bool matchOverloadFun(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		auto decl = ctx.query<QueryDeclOfFun>(fun);

		std::vector<base::Box<Expr>> coerced_arguments;
		usize                        normal_args_position = 0;
		usize                        used_named_args      = 0;

		for (auto& param: decl->parameters) {
			auto get_arg = [&]() -> base::Optional<CRef<Expr>> {
				if (named_arguments.contains(param.name)) {
					used_named_args++;
					return named_arguments.at(param.name).ref();
				} else if (normal_args_position < positional_arguments.size())
					return positional_arguments.at(normal_args_position++).ref();
				else if (param.initial_value.has_value()) {
					return param.initial_value->ref();
				} else
					return std::nullopt;
			};

			match_optional(get_arg()) {
				opt_some(arg) {
					// @TODO: #1029 overloads resolution
					auto coercion = canCoerceExpression(ctx, arg, param.type);
					if (coercion.hasError()) return false;
				}
				opt_none { return false; }
			}
		}

		if (normal_args_position != positional_arguments.size()  // All arguments must be used.
		    or used_named_args != named_arguments.size())
			return false;

		return true;
	}

	/**
	 * Checks if a builtin function can be called with the given arguments for overload resolution.
	 * It has duplicate code with the `attemptFittingBuiltin`, but this function never invalidates
	 * the arguments.
	 * @TODO: #1362 After changing QueryHoutOfExpr to return Ref instead of Box it will be possible
	 * to deduplicate this function with `attemptFittingBuiltin`.
	 */
	bool matchOverloadBuiltin(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		auto call_type_result = ctx.query<QueryTypeOfSymbol>({ fun });
		if (call_type_result->hasError()) return false;

		tsh::SymbolType<tsh::FunctionAbstractType> call_type = call_type_result->value();

		if (call_type.getType().getParameterTypes().size() != positional_arguments.size()
		    || !named_arguments.empty())
			return false;  // Builtin functions don't support named arguments (for now).

		for (usize i = 0; i < call_type.getType().getParameterTypes().size(); ++i) {
			auto coercion = canCoerceExpression(
				ctx, positional_arguments[i].ref(), call_type.getType().getParameterTypes()[i]
			);
			if (coercion.hasError()) return false;
		}
		return true;
	}

	/**
	 * Checks if a function can be called with the given arguments for overload resolution.
	 * It has duplicate code with the `attemptFitting`, but this function never invalidates the
	 * arguments.
	 * @TODO: #1362 After changing QueryHoutOfExpr to return Ref instead of Box it will be possible
	 * to deduplicate this function with `attemptFitting`.
	 */
	bool matchOverload(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		switch (kind(fun)) {
		case SymbolKind::Function:
		case SymbolKind::FunctionDeclaration:
			return matchOverloadFun(ctx, fun, positional_arguments, named_arguments);
		case SymbolKind::BuiltinFunction:
			return matchOverloadBuiltin(ctx, fun, positional_arguments, named_arguments);
		default:
			CORE_PANIC(base::strConcat(
				"Function candidate \"",
				name(fun),
				"\" is not a function, builtin function, or class"
			));
		}
	}

	/**
	 * @brief Attempts to use given positional and named arguments as arguments for given function.
	 * @note invalidates positional and named_arguments (moves boxes and leaves them empty).
	 */
	base::Optional<Box<CallExpr>> attemptFittingFun(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		auto decl = ctx.query<QueryDeclOfFun>(fun);

		std::vector<base::Box<Expr>> coerced_arguments;
		usize                        normal_args_position = 0;
		usize                        used_named_args      = 0;

		for (auto& param: decl->parameters) {
			auto get_arg = [&]() -> base::Optional<Box<Expr>> {
				if (named_arguments.contains(param.name)) {
					used_named_args++;
					return std::move(named_arguments.at(param.name));
				} else if (normal_args_position < positional_arguments.size())
					return std::move(positional_arguments.at(normal_args_position++));
				else if (param.initial_value.has_value()) {
					return param.initial_value->ref()->clone();
				} else
					return std::nullopt;
			};

			match_optional(get_arg()) {
				opt_some(arg) {
					// @TODO: #1029 overloads resolution
					auto coercion = canCoerceExpression(ctx, arg.ref(), param.type);
					if (coercion.hasError())
						return std::nullopt;
					else
						coerced_arguments.push_back(coercion.value()(std::move(arg)));
				}
				opt_none { return std::nullopt; }
			}
		}

		if (normal_args_position != positional_arguments.size()  // All arguments must be used.
		    or used_named_args != named_arguments.size())
			return std::nullopt;

		auto identifier_expr = makeBox<IdentifierExpr>(ctx, fun);
		return makeBox<CallExpr>(ctx, std::move(identifier_expr), std::move(coerced_arguments));
	}

	/**
	 * @brief Attempts to use given positional and named arguments as arguments for given builtin
	 * function.
	 * @note invalidates positional and named_arguments (moves boxes and leaves them empty).
	 * @TODO: #1362 after Box->Ref migration deduplicate with matchOverloadBuiltin.
	 */
	base::Optional<Box<CallExpr>> attemptFittingBuiltin(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		auto call_type_result = ctx.query<QueryTypeOfSymbol>({ fun });
		if (call_type_result->hasError()) return std::nullopt;

		tsh::SymbolType<tsh::FunctionAbstractType> call_type = call_type_result->value();

		if (call_type.getType().getParameterTypes().size() != positional_arguments.size()
		    || !named_arguments.empty())
			return std::nullopt;  // Builtin functions don't support named arguments (for now).

		std::vector<base::Box<Expr>> coerced_arguments;
		for (usize i = 0; i < call_type.getType().getParameterTypes().size(); ++i) {
			auto coercion = canCoerceExpression(
				ctx, positional_arguments[i].ref(), call_type.getType().getParameterTypes()[i]
			);
			if (coercion.hasError())
				return std::nullopt;
			else
				coerced_arguments.push_back(coercion.value()(std::move(positional_arguments[i])));
		}

		auto identifier_expr = makeBox<IdentifierExpr>(ctx, fun);
		return makeBox<CallExpr>(ctx, std::move(identifier_expr), std::move(coerced_arguments));
	}

	/**
	 * @brief Attempts to use given normal and named arguments as arguments for given function.
	 */
	base::Optional<Box<CallExpr>> attemptFitting(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		switch (kind(fun)) {
		case SymbolKind::Function:
		case SymbolKind::FunctionDeclaration:
			return attemptFittingFun(ctx, fun, positional_arguments, named_arguments);
		case SymbolKind::BuiltinFunction:
			return attemptFittingBuiltin(ctx, fun, positional_arguments, named_arguments);
		default:
			CORE_PANIC(base::strConcat(
				"Function candidate \"",
				name(fun),
				"\" is not a function, builtin function, or class"
			));
		}
	}

	/**
	 * @brief Unwraps and validates call arguments from PST, populating positional and named
	 * argument collections.
	 * @param ctx Query context
	 * @param call_expr The PST call expression containing arguments
	 * @param positional_arguments Output vector for positional arguments
	 * @param named_arguments Output map for named arguments
	 * @return QError if validation fails (duplicate names, positional after named, or expression
	 * error)
	 */
	query::QResult<std::monostate, errors::Failed> unwrapCallArguments(
		query::Context&                        ctx,
		pst::Access<pst::expr::Call>           call_expr,
		std::vector<Box<Expr>>&                positional_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		for (auto&& arg: *call_expr->getArgs().unlock(ctx)) {
			auto arg_expr
				= ctx.query<QueryHoutOfExpr>(arg.unlock(ctx)->getArg().unlock(ctx)->getExpr());
			if (arg_expr.hasError()) return query::QError(errors::Failed());
			if (arg.unlock(ctx)->isNamedArg()) {
				base::StrID arg_name = arg.unlock(ctx)->getArgName().value.value();
				if (named_arguments.contains(arg_name))
					return query::QError(errors::Failed());  // Not unique names.
				named_arguments.emplace(arg_name, std::move(arg_expr.value()));
			} else {
				if (!named_arguments.empty())
					return query::QError(errors::Failed());  // Normal argument after named one.
				positional_arguments.emplace_back(std::move(arg_expr.value()));
			}
		}
		return std::monostate{};
	}

	query::QResult<Box<CallExpr>, errors::Failed> processFunctionCall(
		query::Context&              ctx,
		const std::vector<SymID>&    candidates,
		pst::Access<pst::expr::Call> call_expr
	) {
		// Unwrap and validate call arguments.
		std::vector<Box<Expr>>                positional_arguments;
		base::HashMap<base::StrID, Box<Expr>> named_arguments;
		auto                                  unwrap_result
			= unwrapCallArguments(ctx, call_expr, positional_arguments, named_arguments);
		if (unwrap_result.hasError()) return query::QError(errors::Failed());

		if (candidates.size() == 1) {
			auto call_res
				= attemptFitting(ctx, candidates[0], positional_arguments, named_arguments);
			if (call_res.has_value())
				return std::move(call_res.value());
			else
				return query::QError(errors::Failed());
		}

		base::Optional<SymID> successful_candidate;
		for (const auto& fun: candidates) {
			bool match = matchOverload(ctx, fun, positional_arguments, named_arguments);
			if (match) {
				if (successful_candidate.has_value())
					return query::QError(errors::Failed());  // Ambiguous call.
				successful_candidate = fun;
			}
		}
		if (not successful_candidate.has_value())
			return query::QError(errors::Failed());  // No matching overload.
		auto result = attemptFitting(
			ctx, successful_candidate.value(), positional_arguments, named_arguments
		);
		if (result.has_value())
			return std::move(result.value());
		else
			return query::QError(errors::Failed());
	}
}
