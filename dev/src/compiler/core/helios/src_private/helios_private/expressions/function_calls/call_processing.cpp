#include "call_processing.hpp"

#include "errors.hpp"

#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/helios_errors.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <typesystem/higher/symbol_type.hpp>
#include <typesystem/higher/types.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>

#include <vector>

namespace compiler::helios::code {

	// =================== Argument Origin Variants ===================

	/**
	 * After the matching agains a function overload we keep the information about from where each
	 * argument value should come from. E.x. `foo(a: int, b: bool, c: string = "hello")` and
	 * call `foo(10, b=true)` will have the following origins:
	 * - argument 0: PositionalArgumentOrigin(0) - index in the list of positional arguments
	 * - argument 1: NamedArgumentOrigin(0) - index in the list of named arguments
	 * - argument 2: DefaultArgumentOrigin(ref to expression `"hello"`)
	 */

	struct PositionalArgumentOrigin {
		// We can't keep a direct reference to the argument because we would like to move
		// from the positional arguments vector later, so we just keep the index.
		usize index_in_positional_args;
	};

	struct NamedArgumentOrigin {
		// Same as above.
		usize index_in_named_args;
	};

	struct DefaultArgumentOrigin {
		// This is a shortcut to avoid keeping the entire function declaration around.
		CRef<Expr> default_value_expr;
	};

	using ArgumentOrigin
		= std::variant<DefaultArgumentOrigin, NamedArgumentOrigin, PositionalArgumentOrigin>;

	// =================== Match Result Variants ===================

	struct ExactMatch {
		/**
		 * The function that was matched.
		 */
		SymID function;
		/**
		 * The origin of each argument in the call.
		 */
		std::vector<ArgumentOrigin> argument_origin;
	};

	struct CoercionMatch {
		/**
		 * Same as in ExactMatch.
		 */
		SymID function;
		/**
		 * Same as in ExactMatch.
		 */
		std::vector<ArgumentOrigin> argument_origin;
		/**
		 * The coercion that was validated for each argument, because the coercion logic requires
		 * this "coercion ticket" to actually perform the coercion.
		 */
		base::HashMap<usize, Coercion> coercions;
	};

	struct NoMatch {
		MatchFailure reason;
	};

	using MatchResult = std::variant<CoercionMatch, ExactMatch, NoMatch>;

	/**
	 * Checks if a function can be called with the given arguments for overload resolution
	 * based on function declaration.
	 */
	MatchResult matchOverloadFun(
		query::Context&                                        ctx,
		SymID                                                  fun,
		const std::vector<Box<Expr>>&                          positional_arguments,
		const std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments
	) {
		auto                                        decl = ctx.query<QueryDeclOfFun>(fun);
		std::vector<base::Optional<ArgumentOrigin>> argument_origin(decl->parameters.size());
		base::HashMap<usize, Coercion>              coercions;
		bool                                        coercion_present = false;

		if (positional_arguments.size() + named_arguments.size() > decl->parameters.size())
			return NoMatch{ ToManyCallArguments{} };

		// Go over positional arguments.
		for (usize i = 0; i < positional_arguments.size(); i++) {
			tsh::SymbolType provided_type
				= positional_arguments[i]->expression_type.getSymbolType();
			tsh::SymbolType expected_type = decl->parameters[i].type;

			// Currently comparing by abstract type and not symbol type,
			// becasue things get annoying quckly with const / not const
			// (int literal is a const i64, but function might expect i64).
			if (provided_type.getType() != expected_type.getType()) {
				if (auto coercion = canCoerce(ctx, provided_type, expected_type);
				    coercion.hasValue()) {
					coercion_present = true;
					coercions.try_emplace(i, coercion.value());
				} else {
					return NoMatch{ TypeMismatch{} };
				}
			}
			// Position in the parameter list is the same as in the positional arguments list.
			argument_origin[i] = PositionalArgumentOrigin{ .index_in_positional_args = i };
		}


		// Go over named arguments.
		for (usize i{ 0 }; i < named_arguments.size(); i++) {
			base::Optional<usize> param_idx_with_matching_name{};
			for (usize param_idx{ 0 }; param_idx < decl->parameters.size(); param_idx++) {
				if (decl->parameters[param_idx].name == std::get<0>(named_arguments[i])) {
					if (argument_origin[param_idx].has_value())
						return NoMatch{ DuplicateNamedArgument{} };

					param_idx_with_matching_name = param_idx;
					break;
				}
			}

			// Name mismatch case.
			if (param_idx_with_matching_name.empty()) return NoMatch{ UnknownNamedArgument{} };

			usize           param_idx = param_idx_with_matching_name.value();
			tsh::SymbolType provided_type
				= std::get<1>(named_arguments[i])->expression_type.getSymbolType();
			tsh::SymbolType expected_type = decl->parameters[param_idx].type;
			if (provided_type != expected_type) {
				if (auto coercion = canCoerce(ctx, provided_type, expected_type);
				    coercion.hasValue()) {
					coercion_present = true;
					coercions.try_emplace(param_idx, coercion.value());
				} else {
					return NoMatch{ TypeMismatch{} };
				}
			}
			argument_origin[param_idx] = NamedArgumentOrigin{ .index_in_named_args = i };
		}

		// Go over default arguments
		for (usize i{ 0 }; i < decl->parameters.size(); i++) {
			if (not argument_origin[i].has_value()) {
				if (decl->parameters[i].initial_value.has_value())
					argument_origin[i]
						= DefaultArgumentOrigin{ decl->parameters[i].initial_value.value().ref() };
				else
					return NoMatch{ MissingCallArgument{} };
			}
		}

		std::vector<ArgumentOrigin> argument_origin_final
			= argument_origin
		    | std::views::transform([](const base::Optional<ArgumentOrigin>& opt) {
				  return opt.value();
			  })
		    | std::ranges::to<std::vector>();


		if (coercion_present) {
			return CoercionMatch{ .function        = fun,
				                  .argument_origin = std::move(argument_origin_final),
				                  .coercions       = std::move(coercions) };
		}


		return ExactMatch{ .function = fun, .argument_origin = std::move(argument_origin_final) };
	}

	/**
	 * Checks if a builtin function can be called with the given arguments for overload resolution.
	 */
	MatchResult matchOverloadBuiltin(
		query::Context&                                        ctx,
		SymID                                                  fun,
		const std::vector<Box<Expr>>&                          positional_arguments,
		const std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments
	) {
		tsh::SymbolType<tsh::FunctionAbstractType> call_type
			= ctx.query<QueryTypeOfSymbol>({ fun })->value();


		if (positional_arguments.size() < call_type.getType().getParameterTypes().size())
			return NoMatch{ MissingCallArgument{} };
		else if (positional_arguments.size() > call_type.getType().getParameterTypes().size())
			return NoMatch{ ToManyCallArguments{} };

		if (named_arguments.size() > 0) return NoMatch{ UnknownNamedArgument{} };

		std::vector<ArgumentOrigin> argument_origin;

		for (usize i{ 0 }; i < call_type.getType().getParameterTypes().size(); ++i) {
			auto expected_type = call_type.getType().getParameterTypes()[i];
			auto provided_type = positional_arguments[i]->expression_type.getSymbolType();
			if (provided_type.getType() != expected_type.getType()) {
				std::cerr << "builtin invalid type at argument " << i << ": expected "
						  << expected_type.getType().toString() << ", provided "
						  << provided_type.getType().toString() << "\n";
				return NoMatch{ TypeMismatch{} };
			}
			argument_origin.emplace_back(PositionalArgumentOrigin{ .index_in_positional_args = i });
		}

		return ExactMatch{ .function = fun, .argument_origin = std::move(argument_origin) };
	}

	/**
	 * Checks if a function can be called with the given arguments for overload resolution.
	 */
	MatchResult matchOverload(
		query::Context&                                        ctx,
		SymID                                                  fun,
		const std::vector<Box<Expr>>&                          positional_arguments,
		const std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments
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

	Box<CallExpr> constructeCallExpr(
		query::Context&                                  ctx,
		SymID                                            fun,
		std::vector<Box<Expr>>&                          positional_arguments,
		std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments,
		const std::vector<ArgumentOrigin>&               argument_origin,
		const base::HashMap<usize, Coercion>&            coercions
	) {
		std::vector<Box<Expr>> final_arguments;
		final_arguments.reserve(argument_origin.size());

		for (usize i{ 0 }; i < argument_origin.size(); i++) {
			Box<Expr> expr = [&] {
				variant_match(argument_origin[i]) {
					variant_case(PositionalArgumentOrigin, positional_origin) {
						return std::move(
							positional_arguments[positional_origin.index_in_positional_args]
						);
					}
					variant_case(NamedArgumentOrigin, named_argument) {
						return std::move(
							std::get<1>(named_arguments[named_argument.index_in_named_args])
						);
					}
					variant_case(DefaultArgumentOrigin, default_argument) {
						return default_argument.default_value_expr->clone();
					}
				}
				CORE_UNREACHABLE();
			}();

			if (coercions.contains(i)) {
				Box<Expr> coerced_expr = coercions[i].coerce(std::move(expr));
				final_arguments.push_back(std::move(coerced_expr));
			} else {
				final_arguments.push_back(std::move(expr));
			}
		}
		return makeBox<CallExpr>(ctx, makeBox<IdentifierExpr>(ctx, fun), std::move(final_arguments));
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
	query::QResult<std::monostate, PositionalAfterNamedArgument, RepeatedNamedArgument, errors::Failed>
		validateCallArguments(
			query::Context&                                  ctx,
			pst::Access<pst::expr::Call>                     call_expr,
			std::vector<Box<Expr>>&                          positional_arguments,
			std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments
		) {
		for (auto&& arg: *call_expr->getArgs().unlock(ctx)) {
			auto arg_expr
				= ctx.query<QueryHoutOfExpr>(arg.unlock(ctx)->getArg().unlock(ctx)->getExpr());
			if (arg_expr.hasError()) return query::QError(arg_expr.error());

			if (arg.unlock(ctx)->isNamedArg()) {
				base::StrID arg_name = arg.unlock(ctx)->getArgName().value.value();
				named_arguments.emplace_back(arg_name, std::move(arg_expr.value()));
			} else {
				if (!named_arguments.empty())
					return query::QError(PositionalAfterNamedArgument{}
					);  // Normal argument after named one.

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
		std::vector<Box<Expr>>                          positional_arguments;
		std::vector<std::tuple<base::StrID, Box<Expr>>> named_arguments;
		auto                                            verify_result
			= validateCallArguments(ctx, call_expr, positional_arguments, named_arguments);
		if (verify_result.hasError()) return query::QError(errors::Failed{});

		std::vector<ExactMatch>    exact_match;
		std::vector<CoercionMatch> coercion_match;


		for (auto candidate: candidates) {
			MatchResult match
				= matchOverload(ctx, candidate, positional_arguments, named_arguments);

			variant_match(match) {
				variant_case(ExactMatch, data) { exact_match.push_back(std::move(data)); }
				variant_case(CoercionMatch, data) { coercion_match.push_back(std::move(data)); }
				variant_case(NoMatch, data) {
					// If there's only one candidate, report the error.
					// Otherwise, ignore it.
					if (candidates.size() == 1) {
						ctx.log(makeBox<InvalidCallExpression>(call_expr->getSourcePosition()));
						return query::QError(errors::Failed{});
					}
				}
			}
		}

		if (exact_match.size() > 1) {
			ctx.log(makeBox<AmbiguousExactMatches>(call_expr->getSourcePosition()));
			return query::QError(errors::Failed());
		}
		if (exact_match.size() == 1) {
			return constructeCallExpr(
				ctx,
				exact_match.back().function,
				positional_arguments,
				named_arguments,
				exact_match.back().argument_origin,
				{}
			);
		}

		if (coercion_match.size() > 1) {
			ctx.log(makeBox<AmbiguousCoercionMatches>(call_expr->getSourcePosition()));
			return query::QError(errors::Failed());
		}
		if (coercion_match.size() == 1) {
			return constructeCallExpr(
				ctx,
				coercion_match.back().function,
				positional_arguments,
				named_arguments,
				coercion_match.back().argument_origin,
				coercion_match.back().coercions
			);
		}

		ctx.log(makeBox<InvalidCallExpression>(call_expr->getSourcePosition()));
		return query::QError(errors::Failed());
	}
}
