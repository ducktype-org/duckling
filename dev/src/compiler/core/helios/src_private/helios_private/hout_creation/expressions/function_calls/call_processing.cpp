// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/origin.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/coercions/coercion.hpp>
#include <helios/tsh/coercions/coercion_rank.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/function_calls/call_processing.hpp>
#include <helios_private/hout_creation/expressions/function_calls/errors.hpp>
#include <helios_private/hout_creation/expressions/hout_of_subexpr.hpp>
#include <helios_private/hout_creation/expressions/operators.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/message.hpp>
#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

#include <algorithm>
#include <compare>
#include <utility>
#include <vector>

namespace compiler::helios::code {

	// =================== Argument Origin Variants ===================

	/**
	 * After the matching against a function overload we keep the information about from where each
	 * argument value should come from. E.x. `foo(a: int, b: bool, c: string = "hello")` and
	 * call `foo(10, b=true)` will have the following origins:
	 * - argument 0: PositionalArgumentOrigin(0) - index in the list of positional arguments
	 * - argument 1: NamedArgumentOrigin(0) - index in the list of named arguments
	 * - argument 2: DefaultArgumentOrigin(ref to expression `"hello"`)
	 */

	struct PositionalArgumentOrigin final {
		// We can't keep a direct reference to the argument because we would like to move
		// from the positional arguments vector later, so we just keep the index.
		usize index_in_positional_args;
	};

	struct NamedArgumentOrigin final {
		// Same as above.
		usize index_in_named_args;
	};

	struct DefaultArgumentOrigin final {
		// This is a shortcut to avoid keeping the entire function declaration around.
		CRef<Expr> default_value_expr;
	};

	using ArgumentOrigin
		= std::variant<DefaultArgumentOrigin, NamedArgumentOrigin, PositionalArgumentOrigin>;

	// =================== Match Result Variants ===================

	using tsh::coercions::Coercion;
	using tsh::coercions::CoercionRank;
	using tsh::coercions::Rank;

	using ArgumentCoercions = std::vector<base::Optional<Coercion>>;

	/**
	 * @brief Whether @p rank means that the argument is passed as is with no runtime effects, which
	 * at most relaxes the mutability it is seen with.
	 * @TODO: #1488 Revisit this.
	 */
	bool isExactRank(const CoercionRank rank) { return rank.getRank() <= Rank::MutabilityChange; }

	struct Match final {
		/**
		 * The function that was matched.
		 */
		SymID function;

		/**
		 * The origin of each argument in the call.
		 */
		std::vector<ArgumentOrigin> argument_origin;

		/**
		 * The coercion of each argument into its parameter.
		 */
		ArgumentCoercions coercions;

		/**
		 * How well each argument matched its parameter, in the order the arguments were written.
		 */
		std::vector<CoercionRank> argument_ranks;

		/**
		 * @brief Whether this is an exact match.
		 */
		[[nodiscard]]
		bool isExact() const {
			return std::ranges::all_of(argument_ranks, isExactRank);
		}

		/**
		 * @brief Compares the two candidates. The "smaller" one is the better match.
		 *
		 * Candidates are compared argument by argument. A candidate is better only when it's at
		 * least as good everywhere, and better on at least one argument. Two candidates that are
		 * each better at different arguments are unordered.
		 */
		[[nodiscard]]
		std::partial_ordering operator<=>(const Match& other) const {
			CORE_ASSERT(
				argument_ranks.size() == other.argument_ranks.size(), "Argument count should match"
			);

			bool better_somewhere = false;
			bool worse_somewhere  = false;
			for (const auto& [mine, theirs]: std::views::zip(argument_ranks, other.argument_ranks)) {
				better_somewhere = better_somewhere || mine.dominates(theirs);
				worse_somewhere  = worse_somewhere || theirs.dominates(mine);
			}

			if (better_somewhere && worse_somewhere) return std::partial_ordering::unordered;
			if (better_somewhere) return std::partial_ordering::less;
			if (worse_somewhere) return std::partial_ordering::greater;
			return std::partial_ordering::equivalent;
		}
	};

	struct NoMatch final {
		SymID                function;
		FunctionMatchFailure reason;
	};

	using MatchResult = std::variant<Match, NoMatch>;

	/**
	 * Checks if a function can be called with the given arguments for overload resolution
	 * based on function declaration.
	 */
	MatchResult matchOverloadCandidate(
		query::Context&                                        ctx,
		SymID                                                  fun,
		const std::vector<Box<Expr>>&                          positional_arguments,
		const std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments
	) {
		auto& decl = ctx.query<QueryDeclOfFun>(fun)->valueOrThrow();
		std::vector<base::Optional<ArgumentOrigin>> argument_origin(decl.parameters.size());
		ArgumentCoercions                           coercions(decl.parameters.size());
		std::vector<CoercionRank>                   argument_ranks(
            positional_arguments.size() + named_arguments.size(), CoercionRank::identity()
        );

		if (positional_arguments.size() > decl.parameters.size())
			return NoMatch{ .function = fun,
				            .reason
				            = TooManyCallArguments{ .valid_arguments = decl.parameters.size(),
				                                    .total_arguments = positional_arguments.size(),
				                                    .function        = fun } };

		// Go over positional arguments.
		for (usize i{ 0 }; i < positional_arguments.size(); i++) {
			const tsh::SymbolType expected_type = decl.parameters[i].type;
			Coercion              coercion
				= coercions::coercionOf(ctx, positional_arguments[i].ref(), expected_type);

			if (coercion.isRefused()) {
				return NoMatch{ .function = fun,
					            .reason   = ArgumentCoercionFailure{
									  .failed         = coercion,
									  .argument_index = i,
									  .function       = fun,
                                } };
			}

			argument_ranks[i] = coercion.getRank();

			// Position in the parameter list is the same as in the positional arguments list.
			argument_origin[i].emplace(PositionalArgumentOrigin{ .index_in_positional_args = i });
			coercions[i].emplace(std::move(coercion));
		}


		// Go over named arguments.
		for (usize i{ 0 }; i < named_arguments.size(); i++) {
			base::Optional<usize> param_idx_with_matching_name{};
			auto&                 name = std::get<0>(named_arguments[i]);
			for (usize param_idx{ 0 }; param_idx < decl.parameters.size(); param_idx++) {
				if (decl.parameters[param_idx].name == name) {
					if (argument_origin[param_idx].has_value())
						return NoMatch{ .function = fun,
							            .reason   = NamedArgumentProvidedByPositional{
											  .argument_index = positional_arguments.size() + i,
											  .function       = fun } };

					param_idx_with_matching_name = param_idx;
					break;
				}
			}

			// Name mismatch case.
			if (param_idx_with_matching_name.empty())
				return NoMatch{ .function = fun,
					            .reason   = UnknownNamedArgument{ .name = name,
					                                              .argument_index
                                                                = positional_arguments.size() + i,
					                                              .function = fun } };

			const usize           param_idx     = param_idx_with_matching_name.value();
			const tsh::SymbolType expected_type = decl.parameters[param_idx].type;
			Coercion              coercion
				= coercions::coercionOf(ctx, std::get<1>(named_arguments[i]).ref(), expected_type);

			if (coercion.isRefused()) {
				return NoMatch{ .function = fun,
					            .reason   = ArgumentCoercionFailure{
									  .failed         = coercion,
									  .argument_index = positional_arguments.size() + i,
									  .function       = fun,
                                } };
			}

			argument_ranks[positional_arguments.size() + i] = coercion.getRank();

			argument_origin[param_idx] = NamedArgumentOrigin{ .index_in_named_args = i };
			coercions[param_idx].emplace(std::move(coercion));
		}

		// Go over default arguments.
		for (usize i{ 0 }; i < decl.parameters.size(); i++) {
			if (not argument_origin[i].has_value()) {
				if (decl.parameters[i].initial_value.has_value())
					argument_origin[i].emplace(DefaultArgumentOrigin{
						decl.parameters[i].initial_value.value().ref() });
				else
					return NoMatch{ .function = fun,
						            .reason   = MissingCallArgument{ .parameter_index = i,
						                                             .function        = fun } };
			}
		}

		std::vector<ArgumentOrigin> argument_origin_final
			= argument_origin
		    | std::views::transform([](const base::Optional<ArgumentOrigin>& opt) {
				  return opt.value();
			  })
		    | std::ranges::to<std::vector>();

		return Match{ .function        = fun,
			          .argument_origin = std::move(argument_origin_final),
			          .coercions       = std::move(coercions),
			          .argument_ranks  = std::move(argument_ranks) };
	}

	/**
	 * @brief Given function symbol and Box<Expr> of all the arguments and argument origins
	 * constructs a helios Expr representing the call of the function. Construction of the
	 * expressions will move the arguments.
	 * @p argument_origin defines the actual structure of the arguments, while @p
	 * positional_arguments and @p named_arguments define their content.
	 *
	 * @param ctx Query context
	 * @param fun The function symbol being called
	 * @param pst_origin The origins of the expression.
	 * @param call_arguments The arguments of the call. Will be moved.
	 * @param argument_origin The origin of each argument in the call (e.x. first argument is
	 * positional, second is named, third is default, etc.)
	 * @param coercions The coercion to apply to each argument, if there is one
	 *
	 * @return Box<CallExpr> representing the function call
	 */
	Box<Expr> constructCallExpr(
		query::Context&                    ctx,
		SymID                              fun,
		const CallPstOrigin&               pst_origin,
		CallArguments                      call_arguments,
		const std::vector<ArgumentOrigin>& argument_origin,
		const ArgumentCoercions&           coercions
	) {
		std::vector<Box<Expr>> final_arguments;
		final_arguments.reserve(argument_origin.size());

		for (usize i{ 0 }; i < argument_origin.size(); i++) {
			Box<Expr> expr = [&] {
				variant_match(argument_origin[i]) {
					variant_case(PositionalArgumentOrigin, positional_origin) {
						return std::move(
							call_arguments
								.positional_arguments[positional_origin.index_in_positional_args]
						);
					}
					variant_case(NamedArgumentOrigin, named_argument) {
						return std::move(std::get<1>(
							call_arguments.named_arguments[named_argument.index_in_named_args]
						));
					}
					variant_case(DefaultArgumentOrigin, default_argument) {
						return default_argument.default_value_expr->clone();
					}
				}
				CORE_UNREACHABLE();
			}();

			if_opt_some(coercions[i], coercion) {
				expr = coercions::applyCoercion(ctx, coercion, std::move(expr));
			}
			final_arguments.push_back(std::move(expr));
		}
		return makeBox<CallExpr>(
			ctx,
			pst_origin.whole_call_origin,
			makeBox<IdentifierExpr>(ctx, pst_origin.callee_origin, fun),
			std::move(final_arguments)
		);
	}

	/**
	 * @brief Given function symbol (assumed to be a builtin operator) and Box<Expr> of all the
	 * arguments and argument origins, constructs a helios Expr representing the evaluation of
	 * the operator. This will end up being a Builtin(Bi/U)naryExpr, or defer to a CallExpr.
	 * Construction of the expressions will move the arguments.
	 * @p argument_origin defines the actual structure of the arguments, while @p
	 * positional_arguments and @p named_arguments define their content.
	 *
	 * @note May construct a BinaryOperatorExpr or a CallExpr depending on what the HELIoS operator
	 * translates to in the lower level.
	 */
	Box<Expr> constructHOUTBuiltinOpExpr(
		query::Context&          ctx,
		const CallPstOrigin&     pst_origin,
		const SymID              fun,
		CallArguments            call_arguments,
		const ArgumentCoercions& coercions
	) {
		const auto call_origin = pst_origin.whole_call_origin;
		auto  hout_op = ctx.query<QueryRegularBuiltinOperatorSymbols>({})->atMaybe(fun).value()->op;
		auto& arguments = call_arguments.positional_arguments;

		std::vector<Box<Expr>> coerced_args;
		coerced_args.reserve(arguments.size());
		for (usize i{ 0 }; i < arguments.size(); i++) {
			Box<Expr> argument = std::move(arguments.at(i));
			if_opt_some(coercions.at(i), coercion) {
				argument = coercions::applyCoercion(ctx, coercion, std::move(argument));
			}
			coerced_args.emplace_back(std::move(argument));
		}

		variant_match(hout_op) {
			variant_case(BuiltinUnary, unary_op) {
				return makeBox<UnaryOperatorExpr>(
					ctx, call_origin, unary_op, std::move(coerced_args.at(0))
				);
			}
			variant_case(BuiltinBinary, op) {
				return makeBox<BinaryOperatorExpr>(
					ctx, call_origin, op, std::move(coerced_args.at(0)), std::move(coerced_args.at(1))
				);
			}
			variant_case(RegularBuiltinOperator::FunctionCall, call) {
				return makeBox<CallExpr>(
					ctx,
					call_origin,
					makeBox<IdentifierExpr>(ctx, pst_origin.callee_origin, call.function_symbol),
					std::move(coerced_args)
				);
			}
		}
		CORE_UNREACHABLE();
	}

	void appendExactMatchesErrors(
		query::Context&             ctx,
		Box<AmbiguousMatchesError>& main_msg,
		const std::vector<Match>&   exact_matches,
		bool                        as_a_link = false
	) {
		if (exact_matches.empty()) return;
		// We have to differentiate between first candidate because all the other candidates will
		// be attached to it.
		base::Optional<Box<dia::MessageBase>> first_candidate_msg{};
		for (const auto& match: exact_matches) {
			auto candidate_note = [&] -> Box<dia::MessageBase> {
				auto& decl = ctx.query<QueryDeclOfFun>(match.function)->valueOrThrow();

				// @TODO: #2110 unify diagnostics between user-defined and generated functions.
				if_opt_none(decl.origin.getStablePosition()) {
					const auto type = ctx.query<QueryTypeOfSymbol>(match.function)->valueOrThrow();
					return makeBox<dia::PlaceholderError>(
						"Found exact candidate.",
						base::strConcat(
							"Candidate is compiler-generated, with type " + type.toString() + "."
						)
					);
				}
				return makeBox<ExactCandidateNote>(decl.origin.getStablePosition().value());
			}();

			if (not first_candidate_msg.has_value())
				first_candidate_msg.emplace(std::move(candidate_note));
			else
				first_candidate_msg.value()->addAttachedMessage(std::move(candidate_note));
		}

		if (as_a_link)
			main_msg->addExploreExactCandidates(
				exact_matches.size(), std::move(first_candidate_msg).value()
			);
		else
			main_msg->addAttachedMessage(std::move(first_candidate_msg).value());
	}

	void appendCoercibleMatchesErrors(
		query::Context&             ctx,
		Box<AmbiguousMatchesError>& main_msg,
		const std::vector<Match>&   coercible_matches,
		bool                        as_a_link
	) {
		if (coercible_matches.empty()) return;
		// We have to differentiate between first candidate because all the other candidates will
		// be attached to it.
		base::Optional<Box<dia::MessageBase>> first_candidate_msg{};
		for (const auto& match: coercible_matches) {
			auto candidate_note = [&] -> Box<dia::MessageBase> {
				auto& decl = ctx.query<QueryDeclOfFun>(match.function)->valueOrThrow();

				// @TODO: #2110 unify diagnostics between user-defined and generated functions.
				if_opt_none(decl.origin.getStablePosition()) {
					const auto type = ctx.query<QueryTypeOfSymbol>(match.function)->valueOrThrow();
					return makeBox<dia::PlaceholderNote>(
						"Found coercible candidate.",
						"Candidate is compiler-generated, with type " + type.toString() + "."
					);
				}

				auto decl_pos = decl.origin.getStablePosition().value();

				auto result = makeBox<CoercibleCandidateNote>(decl_pos);
				for (usize i{ 0 }; i < match.coercions.size(); i++) {
					if_opt_none(match.coercions[i]) continue;
					const Coercion& coercion = match.coercions[i].value();
					if (not isExactRank(coercion.getRank())) {
						if_opt_none(decl.parameters[i].origin.getStablePosition()) continue;
						auto param_pos = decl.parameters[i].origin.getStablePosition().value();

						auto pm = makeBox<CoercibleCandidateCoercionPointerMessage>(
							coercion.getTarget().toString(),
							coercion.getSource().getSymbolType().toString()
						);
						auto pm_message_id = dia::MessageBase::getUniqueID();
						result->addLinkedMessage(pm_message_id, std::move(pm));

						result->addPointerMessage("coercion", param_pos, pm_message_id);
					}
				}
				return result;
			}();

			if (not first_candidate_msg.has_value())
				first_candidate_msg.emplace(std::move(candidate_note));
			else
				first_candidate_msg.value()->addAttachedMessage(std::move(candidate_note));
		}

		if (as_a_link)
			main_msg->addExploreCoercibleCandidates(
				coercible_matches.size(), std::move(first_candidate_msg).value()
			);
		else
			main_msg->addAttachedMessage(std::move(first_candidate_msg).value());
	}

	void appendFailedMatchesErrors(
		query::Context&             ctx,
		Box<AmbiguousMatchesError>& main_msg,
		const CallPstOrigin&        call_pst_origin,
		const std::vector<NoMatch>& failed_matches,
		bool                        as_a_link
	) {
		if (failed_matches.empty()) return;
		// We have to differentiate between first candidate because all the other candidates will
		// be attached to it.
		base::Optional<Box<dia::MessageBase>> first_candidate_msg{};
		for (const auto& [function, reason]: failed_matches) {
			auto candidate_note = [&] -> Box<dia::MessageBase> {
				auto& decl = ctx.query<QueryDeclOfFun>(function)->valueOrThrow();

				// @TODO: #2110 unify diagnostics between user-defined and generated functions.
				if_opt_none(decl.origin.getStablePosition()) {
					const auto type = ctx.query<QueryTypeOfSymbol>(function)->valueOrThrow();
					return makeBox<dia::PlaceholderNote>(
						"Candidate failed to match.",
						"Candidate is compiler-generated, with type " + type.toString() + "."
					);
				}

				return makeBox<FailedCandidateNote>(decl.origin.getStablePosition().value());
			}();

			candidate_note->addAttachedMessage(createDetailedCallErrorMessage(
				ctx, call_pst_origin.whole_call_origin, call_pst_origin.arguments_origin, reason, true
			));

			if (first_candidate_msg.empty())
				first_candidate_msg.emplace(std::move(candidate_note));
			else
				first_candidate_msg.value()->addAttachedMessage(std::move(candidate_note));
		}

		if (as_a_link)
			main_msg->addExploreFailedCandidates(
				failed_matches.size(), std::move(first_candidate_msg).value()
			);
		else
			main_msg->addAttachedMessage(std::move(first_candidate_msg).value());
	}

	using OverloadResolutionResult
		= std::tuple<SymID, std::vector<ArgumentOrigin>, ArgumentCoercions>;

	/**
	 * Associates a subset of overload candidates with an alternate representation of the call
	 * arguments. Operator methods need their operand prepared as `self`, while standalone and
	 * builtin operators must retain the operand's original value category.
	 */
	struct CandidateArgumentsOverride final {
		const std::vector<SymID>& candidates;
		const CallArguments&      arguments;
	};

	/**
	 * @brief Performs overload resolution for a function call, taking into account coercions,
	 * positional and named arguments. Performs diagnostic logging in case of resolution failure.
	 * Returns the symbol of the function that should be called, the ArgumentOrigins for the
	 * arguments and the coercions which should be applied to them (if any).
	 *
	 * @note Does *NOT* take into account the name of each candidate. Resolution is performed solely
	 * based on how well the arguments match the parameters from the declaration.
	 *
	 * @return The function symbol, the argument origins, and the coercions.
	 */
	query::QResult<OverloadResolutionResult> doOverloadResolution(
		query::Context&                            ctx,
		const std::vector<SymID>&                  candidates,
		const CallArguments&                       call_arguments,
		const CallPstOrigin&                       pst_origin,
		base::Optional<CandidateArgumentsOverride> arguments_override = {}
	) {
		if (candidates.empty()) {
			ctx.logInt(makeBox<NoCandidatesFoundError>(
				pst_origin.whole_call_origin.getStablePosition().value()
			));
			return query::Failed();
		}

		std::vector<Match>   matches;
		std::vector<NoMatch> no_match;

		for (const auto candidate: candidates) {
			const auto& candidate_call_arguments
				= arguments_override.has_value()
			           && std::ranges::contains(arguments_override->candidates, candidate)
			        ? arguments_override->arguments
			        : call_arguments;
			MatchResult match = matchOverloadCandidate(
				ctx,
				candidate,
				candidate_call_arguments.positional_arguments,
				candidate_call_arguments.named_arguments
			);

			variant_match(match) {
				variant_case(Match, data) { matches.push_back(std::move(data)); }
				variant_case(NoMatch, data) {
					no_match.push_back(data);
					// For now ignore it, it is handled by the logic below.
				}
			}
		}

		if (not matches.empty()) {
			const auto is_best = [&](const Match& match) {
				return std::ranges::none_of(matches, [&](const Match& other) {
					return other < match;
				});
			};

			std::vector<usize> best;
			for (usize i = 0; i < matches.size(); i++)
				if (is_best(matches.at(i))) best.push_back(i);

			// Exactly one best candidate. Success.
			if (best.size() == 1) {
				Match& winner = matches.at(best.front());
				return OverloadResolutionResult{
					winner.function,
					std::move(winner.argument_origin),
					std::move(winner.coercions),
				};
			}

			// Otherwise the call is ambiguous
			std::vector<Match> exact;
			std::vector<Match> coercible;
			std::vector<Match> beaten;
			for (usize i = 0; i < matches.size(); i++)
				if (not std::ranges::contains(best, i))
					beaten.push_back(std::move(matches.at(i)));
				else if (matches.at(i).isExact())
					exact.push_back(std::move(matches.at(i)));
				else
					coercible.push_back(std::move(matches.at(i)));

			auto main_msg = makeBox<AmbiguousMatchesError>(
				pst_origin.whole_call_origin.getStablePosition().value()
			);

			if (not exact.empty()) {
				appendExactMatchesErrors(ctx, main_msg, exact, false);
				for (Match& match: beaten) coercible.push_back(std::move(match));
				appendCoercibleMatchesErrors(ctx, main_msg, coercible, true);
			} else {
				appendCoercibleMatchesErrors(ctx, main_msg, coercible, false);
			}

			appendFailedMatchesErrors(ctx, main_msg, pst_origin, no_match, true);
			ctx.logInt(std::move(main_msg));
			return query::Failed();
		}

		if (candidates.size() == 1) {
			ctx.logInt(createDetailedCallErrorMessage(
				ctx, pst_origin.whole_call_origin, pst_origin.arguments_origin, no_match[0].reason, false
			));
		} else {
			auto main_msg = makeBox<AmbiguousMatchesError>(
				pst_origin.whole_call_origin.getStablePosition().value()
			);
			appendFailedMatchesErrors(ctx, main_msg, pst_origin, no_match, false);
			ctx.logInt(std::move(main_msg));
		}

		return query::Failed();
	}

	/**
	 * @brief Unwraps and validates call arguments from PST, populating positional and named
	 * argument collections.
	 * @param ctx Query context
	 * @param call_expr The PST call expression containing arguments
	 * @param[out] positional_arguments Output vector for positional arguments
	 * @param[out] named_arguments Output map for named arguments
	 * @return A vector of argument origins if successful, or a failure result if validation fails.
	 */
	query::QResult<std::vector<ElementOrigin>> fillCallArgs(
		query::Context&                                  ctx,
		pst::Access<pst::expr::Call>                     call_expr,
		ElementOrigin                                    whole_call_origin,
		std::vector<Box<Expr>>&                          positional_arguments,
		std::vector<std::tuple<base::StrID, Box<Expr>>>& named_arguments
	) {
		usize                      arg_index = 0;
		std::vector<ElementOrigin> arguments_origin;

		for (auto&& arg: *call_expr->getArgs().unlock(ctx))
			arguments_origin.emplace_back(pstOrigin(arg.unlock(ctx)));

		for (auto&& arg: *call_expr->getArgs().unlock(ctx)) {
			auto arg_expr_result
				= subExprFromPST(ctx, arg.unlock(ctx)->getArg().unlock(ctx)->getExpr());
			UNPACK_QRESULT_MOVE(auto arg_expr =, arg_expr_result);

			if (arg.unlock(ctx)->isNamedArg()) {
				base::StrID arg_name = arg.unlock(ctx)->getArgName().value().unlock(ctx)->unwrap();
				for (auto&& [existing_name, _]: named_arguments) {
					if (existing_name == arg_name) {
						auto error = RepeatedNamedArgument{ arg_index };
						ctx.logInt(createDetailedCallErrorMessage(
							ctx, whole_call_origin, arguments_origin, error, false
						));
						return query::Failed();
					}
				}
				named_arguments.emplace_back(arg_name, std::move(arg_expr));
			} else {
				if (!named_arguments.empty()) {
					auto error = PositionalAfterNamedArgument{ arg_index };
					ctx.logInt(createDetailedCallErrorMessage(
						ctx, whole_call_origin, arguments_origin, error, false
					));
					return query::Failed();
				}

				positional_arguments.emplace_back(std::move(arg_expr));
			}
			arg_index++;
		}
		return arguments_origin;
	}

	query::QResult<Box<Expr>> processFunctionCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr
	) {
		auto whole_call_origin = multiplePstOriginOrdered({ callee_element, call_expr });

		// Unwrap and validate call arguments.
		std::vector<Box<Expr>>                          positional_arguments;
		std::vector<std::tuple<base::StrID, Box<Expr>>> named_arguments;
		UNPACK_QRESULT(
			auto arguments_origin =,
			fillCallArgs(ctx, call_expr, whole_call_origin, positional_arguments, named_arguments)
		);

		CallArguments call_arguments{
			.positional_arguments = std::move(positional_arguments),
			.named_arguments      = std::move(named_arguments),
		};
		CallPstOrigin pst_origin{
			.whole_call_origin = whole_call_origin,
			.callee_origin     = pstOrigin(callee_element),
			.arguments_origin  = std::move(arguments_origin),
		};

		// Resolve overloads and construct call expression
		auto overload_resolution_qresult
			= doOverloadResolution(ctx, candidates, call_arguments, pst_origin);
		UNPACK_QRESULT(auto overload_resolution_result =, overload_resolution_qresult);
		const auto [callee_sym, argument_origin, coercions] = std::move(overload_resolution_result);

		return constructCallExpr(
			ctx, callee_sym, pst_origin, std::move(call_arguments), argument_origin, coercions
		);
	}

	query::QResult<Box<Expr>> processMethodCall(
		query::Context&               ctx,
		const std::vector<SymID>&     candidates,
		pst::Access<pst::LangElement> callee_element,
		pst::Access<pst::expr::Call>  call_expr,
		Box<Expr>                     self_arg
	) {
		auto whole_call_origin
			= pstOriginOrdered(pstOriginOrdered(self_arg->origin, callee_element), call_expr);

		// Unwrap and validate call arguments.
		std::vector<Box<Expr>>                          positional_arguments;
		std::vector<std::tuple<base::StrID, Box<Expr>>> named_arguments;
		UNPACK_QRESULT(
			auto arguments_origin =,
			fillCallArgs(ctx, call_expr, whole_call_origin, positional_arguments, named_arguments)
		);
		arguments_origin.insert(arguments_origin.begin(), self_arg->origin);
		positional_arguments.insert(positional_arguments.begin(), std::move(self_arg));

		CallArguments call_arguments{
			.positional_arguments = std::move(positional_arguments),
			.named_arguments      = std::move(named_arguments),
		};
		CallPstOrigin pst_origin{
			.whole_call_origin = whole_call_origin,
			.callee_origin     = pstOrigin(callee_element),
			.arguments_origin  = std::move(arguments_origin),
		};

		// Resolve overloads and construct call expression
		auto overload_resolution_qresult
			= doOverloadResolution(ctx, candidates, call_arguments, pst_origin);
		UNPACK_QRESULT(auto overload_resolution_result =, overload_resolution_qresult);
		const auto [callee_sym, argument_origin, coercions] = std::move(overload_resolution_result);

		return constructCallExpr(
			ctx, callee_sym, pst_origin, std::move(call_arguments), argument_origin, coercions
		);
	}

	query::QResult<Box<Expr>> processBinaryOperatorCall(
		query::Context&           ctx,
		const std::vector<SymID>& candidates,
		const std::vector<SymID>& method_candidates,
		Box<Expr>                 lhs,
		Box<Expr>                 rhs,
		ElementOrigin             op_origin
	) {
		// Obtain the call source positions
		const auto lhs_origin = lhs->origin;
		const auto rhs_origin = rhs->origin;

		const auto whole_call_origin = elementOriginOrdered(lhs->origin, rhs->origin);

		const CallPstOrigin pst_origin{
			.whole_call_origin = whole_call_origin,
			.callee_origin     = op_origin,
			.arguments_origin  = { lhs_origin, rhs_origin },
		};
		CallArguments call_arguments{};
		call_arguments.positional_arguments.emplace_back(std::move(lhs));
		call_arguments.positional_arguments.emplace_back(std::move(rhs));
		// Keep the original arguments for standalone and builtin operators. Method operators
		// require a separate expression tree whose left operand is prepared as `self`; the winning
		// candidate must later be constructed with the same representation used to match it.
		CallArguments method_call_arguments{};
		if (not method_candidates.empty()) {
			method_call_arguments.positional_arguments.emplace_back(
				shorthands::Shorthand{ ctx }.prepToPassSelf(
					call_arguments.positional_arguments.at(0)->clone()
				)
			);
			method_call_arguments.positional_arguments.emplace_back(
				call_arguments.positional_arguments.at(1)->clone()
			);
		}
		base::Optional<CandidateArgumentsOverride> method_arguments_override{};
		if (not method_candidates.empty())
			method_arguments_override.emplace(CandidateArgumentsOverride{
				.candidates = method_candidates,
				.arguments  = method_call_arguments,
			});

		// Resolve overloads and construct call expression
		auto overload_resolution_qresult = doOverloadResolution(
			ctx, candidates, call_arguments, pst_origin, method_arguments_override
		);
		UNPACK_QRESULT(auto overload_resolution_result =, overload_resolution_qresult);
		const auto [callee_sym, argument_origin, coercions] = std::move(overload_resolution_result);
		auto& selected_call_arguments = std::ranges::contains(method_candidates, callee_sym)
		                                  ? method_call_arguments
		                                  : call_arguments;

		// Now construct the expression.
		// If the function is a builtin operator, we use special
		// handling which may not be simply a single function call.
		// Note: this does not include numeric operators on numeric arguments,
		// as that case is handled earlier, before considering overload resolution.
		variant_match(getSymRef(callee_sym)->other) {
			variant_case_novalue(defgen::BuiltinOperator) {
				return constructHOUTBuiltinOpExpr(
					ctx, pst_origin, callee_sym, std::move(selected_call_arguments), coercions
				);
			}
			variant_default {}
		}

		// Otherwise, we construct a normal function call expression.
		return constructCallExpr(
			ctx, callee_sym, pst_origin, std::move(selected_call_arguments), argument_origin, coercions
		);
	}

	query::QResult<Box<Expr>> processUnaryOperatorCall(
		query::Context&                        ctx,
		const std::vector<SymID>&              candidates,
		const std::vector<SymID>&              method_candidates,
		Box<Expr>                              inner,
		ElementOrigin                          op_origin,
		HOUTFunctionDeclaration::Operatoriness operatoriness
	) {
		CORE_ASSERT(
			operatoriness == HOUTFunctionDeclaration::Operatoriness::Prefix
				|| operatoriness == HOUTFunctionDeclaration::Operatoriness::Suffix,
			"Given operatoriness must be either prefix or suffix."
		);
		// Obtain the call source positions
		const auto inner_origin = inner->origin;
		const auto whole_call_origin
			= operatoriness == HOUTFunctionDeclaration::Operatoriness::Prefix
		        ? elementOriginOrdered(op_origin, inner_origin)
		        : elementOriginOrdered(inner_origin, op_origin);
		const CallPstOrigin pst_origin{
			.whole_call_origin = whole_call_origin,
			.callee_origin     = op_origin,
			.arguments_origin  = { inner_origin },
		};
		CallArguments call_arguments{};
		call_arguments.positional_arguments.emplace_back(std::move(inner));
		// As for binary operators, only method candidates may see the operand prepared as `self`.
		// Preserve both trees so resolution and construction use the same representation.
		CallArguments method_call_arguments{};
		if (not method_candidates.empty()) {
			method_call_arguments.positional_arguments.emplace_back(
				shorthands::Shorthand{ ctx }.prepToPassSelf(
					call_arguments.positional_arguments.at(0)->clone()
				)
			);
		}
		base::Optional<CandidateArgumentsOverride> method_arguments_override{};
		if (not method_candidates.empty())
			method_arguments_override.emplace(CandidateArgumentsOverride{
				.candidates = method_candidates,
				.arguments  = method_call_arguments,
			});

		// Resolve overloads and construct call expression
		auto overload_resolution_qresult = doOverloadResolution(
			ctx, candidates, call_arguments, pst_origin, method_arguments_override
		);
		UNPACK_QRESULT(auto overload_resolution_result =, overload_resolution_qresult);
		const auto [callee_sym, argument_origin, coercions] = std::move(overload_resolution_result);
		auto& selected_call_arguments = std::ranges::contains(method_candidates, callee_sym)
		                                  ? method_call_arguments
		                                  : call_arguments;

		// Now construct the expression.
		// If the function is a builtin operator, we use special
		// handling which may not be simply a single function call.
		variant_match(getSymRef(callee_sym)->other) {
			variant_case(defgen::BuiltinOperator, generated) {
				return constructHOUTBuiltinOpExpr(
					ctx, pst_origin, callee_sym, std::move(selected_call_arguments), coercions
				);
			}
		}

		// Otherwise, we construct a normal function call expression.
		return constructCallExpr(
			ctx, callee_sym, pst_origin, std::move(selected_call_arguments), argument_origin, coercions
		);
	}
}
