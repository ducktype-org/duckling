#include "coercions.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <pst_parser/elements/hierarchy/expr_holders.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>

#include <base/box.hpp>
#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>

#include <vector>

namespace compiler::helios::code {

	/**
	 * @brief Attemps to use given normal and named arguments as arguments for given function.
	 * @note invalidates normal and named_arguments (may move expr from boxes and leave them empty).
	 * @TODO: #1029 in order to handle overloads, make normal_arguments and named_arguments not get invalidated.
	 */
	base::Optional<Box<CallExpr>> attempFittingFun(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                normal_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		auto                         decl = ctx.query<QueryDeclOfFun>(fun);
		std::vector<base::Box<Expr>> coerced_arguments;
		usize                        normal_args_position = 0, used_named_args = 0;
		for (auto& param: *(decl.parameters)) {
			auto get_arg = [&]() -> base::Optional<Box<Expr>> {
				if (named_arguments.contains(param.name))
					return ++used_named_args, std::move(named_arguments[param.name]);
				else if (normal_args_position < normal_arguments.size())
					return std::move(normal_arguments[normal_args_position++]);
				else if (param.initial_value.has_value())
					return std::move(param.initial_value.value());
				else
					return std::nullopt;
			};

			match_optional(get_arg()) {
				opt_some(arg) {
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
	 * @note invalidadates normal and named_arguments (may move expr from boxes and leave them empty).
	 * @TODO: #1029 in order to handle overloads, make normal_arguments and named_arguments not get invalidated.
	 */
	base::Optional<Box<CallExpr>> attempFittingBuiltin(
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
			return std::nullopt;  // Builtin functions doesn't support named arguments.

		std::vector<base::Box<Expr>> coerced_arguments;
		for (usize i = 0; i < call_type.getType().getParameterTypes().size(); ++i) {
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
	base::Optional<Box<CallExpr>> attempFitting(
		query::Context&                        ctx,
		SymID                                  fun,
		std::vector<Box<Expr>>&                normal_arguments,
		base::HashMap<base::StrID, Box<Expr>>& named_arguments
	) {
		switch (kind(fun)) {
		case SymbolKind::Function:
			return attempFittingFun(ctx, fun, normal_arguments, named_arguments);
		case SymbolKind::BuiltinFunction:
			return attempFittingBuiltin(ctx, fun, normal_arguments, named_arguments);
		default:
			CORE_PANIC("Function candidate is neither a function nor a builtin function");
		}
	}

	base::Optional<Box<CallExpr>> processFunctionCall(
		query::Context&              ctx,
		const std::vector<SymID>     candidates,
		pst::Access<pst::expr::Call> call_expr
	) {
		CORE_ASSERT(candidates.size() == 1, "Overloading is not implemented yet");

		// Unwrap and validate call arguments.
		std::vector<Box<Expr>>                normal_arguments;
		base::HashMap<base::StrID, Box<Expr>> named_arguments;
		for (auto&& arg: *call_expr->getArgs().unlock(ctx)) {
			auto arg_expr
				= ctx.query<QueryHoutOfExpr>({ arg.unlock(ctx)->arg.give().unlock(ctx)->getExpr() });
			if (arg_expr.hasError()) return std::nullopt;
			if (arg.unlock(ctx)->arg_name.has_value()) {
				base::StrID arg_name = arg.unlock(ctx)->arg_name.value().value;
				if (named_arguments.contains(arg_name)) return std::nullopt;  // Not unique names.
				named_arguments.emplace(std::move(arg_name), std::move(arg_expr.value()));
			} else {
				if (!named_arguments.empty())
					return std::nullopt;  // Normal argument after named one.
				normal_arguments.emplace_back(std::move(arg_expr.value()));
			}
		}

		base::Optional<Box<CallExpr>> result;
		for (const auto& fun: candidates) {
			match_optional(attempFitting(ctx, fun, normal_arguments, named_arguments)) {
				opt_some(call_res) {
					if (result.has_value())
						return std::nullopt;  // At least 2 functions fit.
					else
						result = std::move(call_res);
				}
			}
		}
		return result;
	}
}
