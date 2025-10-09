#include "call_processing.hpp"

#include "coercions.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <pst_parser/elements/hierarchy/expr_holders.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <typesystem/higher/types.hpp>

#include <base/box.hpp>
#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>

#include <query_framework/context.hpp>

#include <vector>

namespace compiler::helios::code {

	/**
	 * @brief Attemps to use given normal and named arguments as arguments for given function.
	 * @note invalidates normal and named_arguments (may move expr from boxes and leave them empty).
	 * @TODO: #1029 in order to handle overloads, make normal_arguments and named_arguments not get
	 * invalidated. Requires #1309.
	 */
	base::Optional<Box<CallExpr>> attemptFittingFun(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                normal_arguments,
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
				} else if (normal_args_position < normal_arguments.size())
					return std::move(normal_arguments.at(normal_args_position++));
				else if (param.initial_value.has_value()) {
					return param.initial_value->ref()->clone();
				} else
					return std::nullopt;
			};

			match_optional(get_arg()) {
				opt_some(arg) {
					// @TODO: #1029 overloads resolution
					auto coerced = coerceExpression(std::move(arg), param.type);
					if (coerced.hasError())
						return std::nullopt;
					else
						coerced_arguments.push_back(std::move(coerced.value()));
				}
				opt_none { return std::nullopt; }
			}
		}

		if (normal_args_position != normal_arguments.size()  // All arguments must be used.
		    or used_named_args != named_arguments.size())
			return std::nullopt;

		auto identifier_expr = makeBox<IdentifierExpr>(ctx, fun);
		return makeBox<CallExpr>(ctx, std::move(identifier_expr), std::move(coerced_arguments));
	}

	/**
	 * @brief Attemps to use given normal and named arguments as arguments for given builtin function.
	 * @note invalidates normal and named_arguments (may move expr from boxes and leave them empty).
	 * @TODO: #1029 in order to handle overloads, make normal_arguments and named_arguments not get
	 * invalidated.
	 */
	base::Optional<Box<CallExpr>> attemptFittingBuiltin(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                normal_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		auto call_type_result = ctx.query<QueryTypeOfSymbol>({ fun });
		if (call_type_result->hasError()) return std::nullopt;

		tsh::SymbolType<tsh::FunctionAbstractType> call_type = call_type_result->value();

		if (call_type.getType().getParameterTypes().size() != normal_arguments.size()
		    || !named_arguments.empty())
			return std::nullopt;  // Builtin functions don't support named arguments (for now).

		std::vector<base::Box<Expr>> coerced_arguments;
		for (usize i = 0; i < call_type.getType().getParameterTypes().size(); ++i) {
			// @TODO: #1300 (for consideration)
			auto coerced = coerceExpression(
				std::move(normal_arguments[i]), call_type.getType().getParameterTypes()[i]
			);
			if (coerced.hasError()) return std::nullopt;
			coerced_arguments.emplace_back(std::move(coerced.value()));
		}

		auto identifier_expr = makeBox<IdentifierExpr>(ctx, fun);
		return makeBox<CallExpr>(ctx, std::move(identifier_expr), std::move(coerced_arguments));
	}

	/**
	 * @brief Attemps to use given normal and named arguments as arguments for given function.
	 */
	base::Optional<Box<CallExpr>> attemptFitting(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                normal_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		switch (kind(fun)) {
		case SymbolKind::Function:
			return attemptFittingFun(ctx, fun, normal_arguments, named_arguments);
		case SymbolKind::BuiltinFunction:
			return attemptFittingBuiltin(ctx, fun, normal_arguments, named_arguments);
		default:
			CORE_PANIC("Function candidate is neither a function nor a builtin function");
		}
	}

	query::QResult<Box<CallExpr>, errors::Failed> processFunctionCall(
		query::Context&              ctx,
		const std::vector<SymID>&    candidates,
		pst::Access<pst::expr::Call> call_expr
	) {
		if (candidates.size() != 1)
			throw base::NotYetImplemented("Overloading is not implemented yet");

		// Unwrap and validate call arguments.
		std::vector<Box<Expr>>                normal_arguments;
		base::HashMap<base::StrID, Box<Expr>> named_arguments;
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
				normal_arguments.emplace_back(std::move(arg_expr.value()));
			}
		}

		base::Optional<Box<CallExpr>> result;
		for (const auto& fun: candidates) {
			match_optional(attemptFitting(ctx, fun, normal_arguments, named_arguments)) {
				opt_some(call_res) {
					if (result.has_value())
						// @TODO: #1029 add proper diagnostic here
						return query::QError(errors::Failed());  // At least 2 functions fit.
					else
						result = std::move(call_res);
				}
			}
		}
		if (result.has_value())
			return std::move(result.value());
		else
			return query::QError(errors::Failed());
	}
}
