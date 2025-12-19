#include "comp_time.hpp"

#include "helios/hout/elements/expr.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <backends/dvm/dvm_backend.hpp>
#include <ctv/ctv.hpp>
#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios_private/comp_time/vm_evaluator.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <typesystem/higher/queries/types.hpp>

#include "base/except/exceptions.hpp"

#include <query_framework/context.hpp>
#include <query_framework/query_impl.hpp>

#include <cmath>
#include <expected>
#include <ranges>
#include <type_traits>
#include <vector>

namespace compiler::helios {
	using namespace ctv;

	struct IMPLEMENT_QUERY(QueryEvaluateHOUTExpression, CompTimeEvalResult) {
		/**
		 * @brief Error indicating that an expression was to complex for a simple tree evaluation.
		 */
		struct CouldNotShortPath {};

		using TreeEvalResult = query::QResult<CompileTimeValue, CouldNotShortPath, query::Failed>;

		/**
		 * @brief A HOUT visitor for compile-time expression evaluation.
		 */
		struct TreeEvalVisitor final: public code::HoutExprVisitor {
			query::Context& ctx;
			TreeEvalResult  result;

			TreeEvalVisitor(query::Context& ctx): ctx(ctx) {}

			TreeEvalResult evaluateSubExpr(CRef<code::Expr> expr) {
				expr->acceptVisitor(*this);
				return std::move(result);
			}

			void visitLiteralUnitExpr(const code::LiteralUnitExpr&) final {
				result = CompileTimeValue{ CompileTimeValue::UnitCTV{} };
			}

			void visitLiteralNumericExpr(const code::LiteralNumericExpr& expr) final {
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralBoolExpr(const code::LiteralBoolExpr& expr) final {
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralStringExpr(const code::LiteralStringExpr&) final {
				throw base::NotYetImplemented("Evaluation of string values in compile time");
			}

			void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) final {
				result = CompileTimeValue{ expr.value_type };
			}

			void visitCallExpr(const code::CallExpr&) final {
				result = query::QError(CouldNotShortPath{});
			}

			void visitAccessExpr(const code::AccessExpr&) final {
				result = query::QError(CouldNotShortPath{});
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
				// Type Evaluation.
				if (expr.expression_type.getType().getKind() == tsh::Kind::Meta) {
					auto type = ctx.query<QueryTypeFromDefinition>({ expr.symbol });
					result    = type->hasValue()
					              ? CompTimeEvalResult{ CompileTimeValue{ type->valueOrThrow() } }
					              : query::QError(query::Failed());
				} else {
					// Constant Evaluation.
					auto const_val_result = ctx.query<QueryConstValueOf>({ expr.symbol });
					result                = const_val_result.hasValue()
					                          ? CompTimeEvalResult{ const_val_result.valueOrThrow() }
					                          : query::QError(query::Failed());
				}
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
				using enum code::BuiltinBinary;
				auto lhs_result = evalHoutExpr(ctx, expr.lhs.ref());
				if (lhs_result.hasError()) {
					result = query::QError(query::Failed());
					return;
				}

				auto rhs_result = evalHoutExpr(ctx, expr.rhs.ref());
				if (rhs_result.hasError()) {
					result = query::QError(query::Failed());
					return;
				}

				const auto& lhs_ctv = lhs_result.valueOrThrow();
				const auto& rhs_ctv = rhs_result.valueOrThrow();

				result = std::visit(
					[&](auto&& lhs, auto&& rhs) -> TreeEvalResult {
						using LhsT = std::decay_t<decltype(lhs)>;
						using RhsT = std::decay_t<decltype(rhs)>;

						// Binary operation on numeric literals.
						if constexpr (std::is_same_v<LhsT, NumericValue>
					                  && std::is_same_v<RhsT, NumericValue>) {
							return std::visit(
								[&](auto&& lhs_val) -> TreeEvalResult {
									using LhsNumT = std::decay_t<decltype(lhs_val)>;

									// @note: We assume both sides of the binary operation have the
							        // same types. If types differ, they should be casted with the
							        // cast expr beforehand.
									auto maybe_rhs_val = rhs.template get<LhsNumT>();
									if (!maybe_rhs_val.has_value()) {
										CORE_PANIC(base::strConcat(
											"Operands on binary expression evaluated at "
											"compile "
											"time are of different type. This should be "
											"prevented by casts.\nLeft side is:",
											lhs.getTypeOfStoredValue(ctx).getType().toString(),
											"\nRight side is: ",
											rhs.getTypeOfStoredValue(ctx).getType().toString()
										));
									}

									LhsNumT rhs_val = maybe_rhs_val.value();

									using ResultT = LhsNumT;
									ResultT result;
									switch (expr.operation) {
									case IntegerAdd:
									case FloatAdd:
										result = lhs_val + rhs_val;
										break;
									case IntegerSub:
									case FloatSub:
										result = lhs_val - rhs_val;
										break;
									case IntegerMul:
									case FloatMul:
										result = lhs_val * rhs_val;
										break;
									case IntegerDiv:
									case FloatDiv:
										if (rhs_val == 0) return query::QError(query::Failed());
										result = lhs_val / rhs_val;
										break;
									case IntegerMod:
									case FloatMod:
										if (rhs_val == 0) return query::QError(query::Failed());
										if constexpr (std::is_integral_v<ResultT>)
											result = lhs_val % rhs_val;
										else
											result = std::fmod(lhs_val, rhs_val);
										break;
									case IntegerPow:
									case FloatPow:
										result = static_cast<ResultT>(std::pow(lhs_val, rhs_val));
										break;
									default:
										throw base::NotYetImplemented(
											"Evaluation of other binary operators in compile time"
										);
									}

									return CompileTimeValue{ NumericValue{ result } };
								},
								lhs.getStorage()
							);
						} else if constexpr (std::is_same_v<LhsT, bool>
					                         && std::is_same_v<RhsT, bool>) {
							using enum code::BuiltinBinary;
							switch (expr.operation) {
							case code::BuiltinBinary::BooleanAnd:
								return CompileTimeValue{ lhs && rhs };
							case code::BuiltinBinary::BooleanOr:
								return CompileTimeValue{ lhs || rhs };
							default:
								throw base::NotYetImplemented("Other binary operators for bool type"
							    );
							}
						} else {
							// Unsupported type for binary operator.
							return query::QError(query::Failed());
						}
					},
					lhs_ctv.getStorage(),
					rhs_ctv.getStorage()
				);
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
				auto expr_result = evalHoutExpr(ctx, expr.expr.ref());
				if (expr_result.hasError()) {
					result = query::QError(query::Failed());
					return;
				}

				const auto& ctv = expr_result.valueOrThrow();


				result = std::visit(
					[&](auto&& val) -> TreeEvalResult {
						using T = std::decay_t<decltype(val)>;
						using enum code::BuiltinUnary;

						if constexpr (std::is_same_v<T, NumericValue>) {
							switch (expr.operation) {
							case IntegerNegation:
							case FloatNegation:
								return std::visit(
									[&](auto&& num_val) -> TreeEvalResult {
										using NumT = std::decay_t<decltype(num_val)>;
										if constexpr (std::is_unsigned_v<NumT>)
											return query::QError(query::Failed());
										else {
											// @note: static cast is needed here. Since cpp
									        // automatically promotes small int types to i32 if any
									        // operation if performed on them.
											return CompileTimeValue{ NumericValue{
												static_cast<NumT>(-num_val) } };
										}
									},
									val.getStorage()
								);

							default:
								throw base::NotYetImplemented(
									"Evaluation of other unary operators for arithmetic types "
									"is not implemented yet"
								);
							}
						} else if constexpr (std::is_same_v<T, bool>) {
							switch (expr.operation) {
							case code::BuiltinUnary::BooleanNot:
								return CompileTimeValue{ not val };
							default:
								return query::QError(query::Failed());
							}

						} else if constexpr (std::is_same_v<T, tsh::SymbolType<>>) {
							switch (expr.operation) {
							case Ref:
								return ctv::CompileTimeValue{
									val.withReferenceKind(tsh::ReferenceKind::Ref)
								};
							case Box:
								return ctv::CompileTimeValue{
									val.withReferenceKind(tsh::ReferenceKind::Box)
								};
							case Const:
								return ctv::CompileTimeValue{
									val.withMutability(tsh::Mutability::Immutable)
								};
							default:
								throw base::NotYetImplemented(
									"Evaluation of other unary operators for tsh::SymbolType<> is "
									"not "
									"implemented yet"
								);
							}
						} else {
							throw base::NotYetImplemented(
								"Evaluation of unary operators for other types is not implemented "
								"yet"
							);
						}
					},
					ctv.getStorage()
				);
			}

			void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) final {
				auto cond_result = evalHoutExpr(ctx, expr.condition.ref());
				if (cond_result.hasError()) {
					result = query::QError(query::Failed());
					return;
				}

				bool        condition_is_true = false;
				const auto& cond_ctv          = cond_result.valueOrThrow();
				match_optional(cond_ctv.get<bool>()) {
					opt_some(value) { condition_is_true = value; }
					opt_none {
						CORE_PANIC(
							"Ternary operator got a non boolean value when evaluating the ternary "
							"operator condition"
						);
					}
				}

				if (condition_is_true)
					result = evalHoutExpr(ctx, expr.if_true.ref());
				else
					result = evalHoutExpr(ctx, expr.if_false.ref());
			}

			void visitChainComparisonExpr(const code::ChainComparisonExpr& chain_expr) final {
				auto compare = [this](
								   const CompileTimeValue& first,
								   const CompileTimeValue& second,
								   code::BuiltinBinary     operation
							   ) {
					return std::visit(
						[&](auto&& lhs_val, auto&& rhs_val) -> bool {
							using LhsT = std::decay_t<decltype(lhs_val)>;
							using RhsT = std::decay_t<decltype(rhs_val)>;

							if constexpr (std::is_same_v<LhsT, NumericValue>
						                  && std::is_same_v<RhsT, NumericValue>) {
								return std::visit(
									[&](auto&& lhs_num) -> bool {
										using LhsNumT = std::decay_t<decltype(lhs_num)>;

										// @note: We assume both sides of the binary operation have
								        // the same types. If types differ, they should be casted
								        // with the cast expr beforehand.
										auto maybe_rhs_val = rhs_val.template get<LhsNumT>();
										if (!maybe_rhs_val.has_value()) {
											CORE_PANIC(base::strConcat(
												"Operands on binary expression evaluated at "
												"compile "
												"time are of different type. This should be "
												"prevented by casts.\nLeft side is:",
												first.getTypeOfStoredValue(ctx).getType().toString(),
												"\nRight side is: ",
												second.getTypeOfStoredValue(ctx).getType().toString()
											));
										}

										LhsNumT rhs_num = maybe_rhs_val.value();
										using enum code::BuiltinBinary;
										switch (operation) {
										case IntegerLt:
										case FloatLt:
											return lhs_num < rhs_num;
										case IntegerGt:
										case FloatGt:
											return lhs_num > rhs_num;
										case IntegerLteq:
										case FloatLteq:
											return lhs_num <= rhs_num;
										case IntegerGteq:
										case FloatGteq:
											return lhs_num >= rhs_num;
										case IntegerEq:
										case FloatEq:
											return lhs_num == rhs_num;
										case IntegerNeq:
										case FloatNeq:
											return lhs_num != rhs_num;
										default:
											CORE_UNREACHABLE();
										}
									},
									lhs_val.getStorage()
								);
							} else if constexpr (std::is_same_v<LhsT, tsh::SymbolType<>>
						                         && std::is_same_v<RhsT, tsh::SymbolType<>>) {
								using enum code::BuiltinBinary;
								switch (operation) {
								case code::BuiltinBinary::MetaEq:
									return lhs_val == rhs_val;
								case code::BuiltinBinary::MetaNeq:
									return lhs_val != rhs_val;
								default:
									CORE_UNREACHABLE();
								}
							} else {
								CORE_PANIC("Unsupported types in CTE chain expr");
							}
						},
						first.getStorage(),
						second.getStorage()
					);
				};

				using namespace std::views;

				auto evaluate_subexpr = [this](const base::Box<code::Expr>& expr) {
					return evalHoutExpr(ctx, expr.ref());
				};

				// Each expression is evaluated lazily, when it becomes useful.
				auto evaluated_exprs = chain_expr.expressions | transform(evaluate_subexpr);

				auto evaluated = evaluate_subexpr(chain_expr.expressions.front());
				if (evaluated.hasError()) {
					result = query::QError(query::Failed());
					return;
				}
				auto prev_value = evaluated.valueOrThrow();
				for (auto [next_expr, comp]: zip(evaluated_exprs | drop(1), chain_expr.operators)) {
					if (next_expr.hasError()) {
						result = query::QError(query::Failed());
						return;
					}

					auto next_value = next_expr.valueOrThrow();
					if (!compare(prev_value, next_value, comp)) {
						result = CompileTimeValue{ false };
						return;
					}

					prev_value = next_value;
				}
				result = CompileTimeValue{ true };
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
				result = evalHoutExpr(ctx, expr.inner.ref());
			}

			void visitTupleExpr(const code::TupleExpr& expr) final {
				std::vector<CompileTimeValue> ctv_elements;

				for (auto& sub_expr: expr.elements) {
					const auto ctv_element_result = evalHoutExpr(ctx, sub_expr.ref());
					if (ctv_element_result.hasError()) {
						result = query::QError(query::Failed(ctv_element_result.error()));
						return;
					}
					ctv_elements.emplace_back(ctv_element_result.valueOrThrow());
				}

				result = CompileTimeValue{ CompileTimeValue::TupleCTV{ std::move(ctv_elements) } };
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) final {
				std::vector<tsh::SymbolType<>> subtypes;
				for (auto& sub_type: expr.subtypes) {
					auto coercion_qresult
						= canCoerceToMeta(ctx, sub_type->expression_type.getSymbolType());
					if (coercion_qresult.hasError()) {
						// @TODO: #1620 report error properly when HOUT exposes source positions.
						result = query::QError(query::Failed());
						return;
					}
					const auto sub_type_coerced
						= coercion_qresult.valueOrThrow().coerce(ctx, sub_type->clone());

					const auto sub_type_ctv
						= ctx.query<QueryEvaluateHOUTExpression>({ sub_type_coerced.ref() });
					if (sub_type_ctv.hasError()) {
						result = query::QError(query::Failed());
						return;
					}

					subtypes.emplace_back(
						sub_type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value()
					);
				}

				result = CompileTimeValue{ tsh::SymbolType<>{
					ctx.query<tsh::QueryVariantType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				} };
			}

			void visitSequenceExpr(const code::SequenceExpr& seq) final {
				result = evalHoutExpr(ctx, seq.expressions.back().ref());
			}

			void visitCastExpr(const code::CastExpr& cast) final {
				// @note: We assume that if we got here, then the cast is valid.
				auto expr_to_cast = evalHoutExpr(ctx, cast.source_expr.ref());
				if (expr_to_cast.hasError()) {
					result = query::QError(query::Failed());
					return;
				}
				const auto& ctv     = expr_to_cast.valueOrThrow();
				const auto& numeric = ctv.get<NumericValue>();
				if (!numeric) CORE_PANIC("Cast expression on a non numeric type");

				auto maybe_new_numeric = numeric->castTo(cast.target_type);
				result                 = maybe_new_numeric.has_value()
				                           ? CompTimeEvalResult{ maybe_new_numeric.value() }
				                           : query::QError(query::Failed());
			}

			/**
			 * @brief Recursively lifts a CompileTimeValue representing a type, a tuple of types,
			 * or a unit to a type.
			 * @note Assumes that the CTV can be lifted to a type, because it assumes that
			 * this has been checked beforehand (e.g. by coercions). Panics if this is not the case.
			 */
			static tsh::SymbolType<> liftCTVToTypeRecursively(
				query::Context& ctx, const CompileTimeValue& ctv
			) {
				variant_match(ctv.getStorage()) {
					variant_case(tsh::SymbolType<>, symbol_type) { return symbol_type; }
					variant_case_novalue(CompileTimeValue::UnitCTV) {
						return tsh::SymbolType<>{
							ctx.query<tsh::QueryUnitType>({}),
							tsh::ReferenceKind::Direct,
							tsh::Mutability::Mutable,
						};
					}
					variant_case(CompileTimeValue::TupleCTV, tuple) {
						std::vector<tsh::SymbolType<>> element_types;
						element_types.reserve(tuple.getElements().size());
						for (const auto& sub_ctv: tuple.getElements())
							element_types.emplace_back(liftCTVToTypeRecursively(ctx, sub_ctv));
						return tsh::SymbolType<>{
							ctx.query<tsh::QueryTupleType>({ std::move(element_types) }),
							tsh::ReferenceKind::Direct,
							tsh::Mutability::Mutable,
						};
					}
				}
				CORE_UNREACHABLE();
			}

			void visitLiftToTypeExpr(const code::LiftToTypeExpr& lift) final {
				auto ctv_to_lift = evalHoutExpr(ctx, lift.value_expr.ref());
				if (ctv_to_lift.hasError()) {
					result = query::QError(query::Failed());
					return;
				}
				// Panics if the CTV cannot be lifted to a type.
				// This is fine, because we assume that this has been checked beforehand by HOUT.
				result
					= CompileTimeValue(liftCTVToTypeRecursively(ctx, ctv_to_lift.valueOrThrow()));
			}
		};

		struct LIRBuildResult {
			std::string                      func_to_call;
			std::vector<CRef<lir::Function>> functions;
		};

		/**
		 * @brief Prepares all necessary LIR functions (dependencies + target) for the VM.
		 * @TODO: #826 Change this code to a single query once it gets implemented.
		 */
		static query::QResult<LIRBuildResult, query::Failed> prepareLIRForDVM(
			query::Context& ctx, SymID function_sym_id
		) {
			// Collect all function dependencies for this function. All functions needed in
			// order to evaluate this one.
			auto dependencies = ctx.query<QueryTransitiveFunctionCalls>(function_sym_id);

			LIRBuildResult result;
			result.functions.reserve(dependencies->size());

			for (const SymID& func_id: *dependencies) {
				auto hout_func_result = ctx.query<QueryCodeOfFun>(func_id);

				auto mir_func_result = ctx.query<mir::LowerToMIRFunction>({ hout_func_result });
				if (mir_func_result->hasError()) return query::QError(mir_func_result->error());

				CRef<mir::Function> mir_func = &mir_func_result->valueOrThrow();
				auto lir_func_result         = ctx.query<lir::LowerToLIRFunction>({ mir_func });

				// When lowering the top level function, we store it's mangled name to know
				// which function to call in the VM.
				if (func_id == function_sym_id)
					result.func_to_call = lir_func_result->mangled_name.str();

				result.functions.push_back(lir_func_result);
			}
			return result;
		}

		/**
		 * @brief Evaluates all argument expressions to CompileTimeValues.
		 */
		static query::QResult<std::vector<CompileTimeValue>, query::Failed> evaluateArguments(
			query::Context& ctx, const std::vector<Box<code::Expr>>& args
		) {
			std::vector<CompileTimeValue> ctv_arguments;
			ctv_arguments.reserve(args.size());

			for (const auto& arg_expr: args) {
				auto arg_result = evalHoutExpr(ctx, arg_expr.ref());
				if (arg_result.hasError()) return query::QError(query::Failed());
				ctv_arguments.push_back(arg_result.valueOrThrow());
			}

			return ctv_arguments;
		}

		/**
		 * @brief Evaluates a HOUT call expression using DVM Eval.
		 * @return The calculated result represented by CompileTimeValue or a Failed error.
		 */
		static CompTimeEvalResult evaluateFunctionWithVm(
			query::Context& ctx, CRef<code::CallExpr> call_expr
		) {
			const auto* callee_ident
				= dynamic_cast<const code::IdentifierExpr*>(call_expr->callee.get());
			if (!callee_ident) return query::QError(query::Failed());
			const SymID function_sym_id = callee_ident->symbol;

			auto args_result = evaluateArguments(ctx, call_expr->arguments);
			if (args_result.hasError()) return query::QError(query::Failed());
			auto ctv_arguments = std::move(args_result.valueOrThrow());

			auto lir_build_result = prepareLIRForDVM(ctx, function_sym_id);
			if (lir_build_result.hasError()) return query::QError(query::Failed());
			const auto& [func_to_call_name, all_lir_functions] = lir_build_result.valueOrThrow();


			// Retrieve the functions return type.
			auto callee_abs_type = callee_ident->expression_type.getSymbolType().getType();
			if (callee_abs_type.getKind() != tsh::Kind::Function) {
				CORE_PANIC(
					"Attempting to call a non_function type during VM compile time evaluation"
				);
			}
			tsh::FunctionAbstractType func_type(callee_abs_type);

			auto vm_eval_result = executeInVm(
				ctx, func_to_call_name, all_lir_functions, ctv_arguments, func_type.getResultType()
			);

			if (!vm_eval_result) return query::QError(query::Failed());
			return vm_eval_result.value();
		}

		/**
		 * @brief Evaluates a HOUT expression using TreeEval.
		 * @return The calculated result represented by CompileTimeValue, a CouldNotShortPath
		 * error if the expresion was to complicated for tree eval or a Failed error.
		 */
		static auto evaluateWithTreeEval(query::Context& ctx, CRef<code::Expr> expr)
			-> TreeEvalResult {
			TreeEvalVisitor visitor(ctx);
			expr->acceptVisitor(visitor);
			return visitor.result;
		}

		/**
		 * @brief Evaluates a HOUT expression using TreeEval or VMEval if the expression is to
		 * complex for tree eval.
		 * @return The calculated result represented by CompileTimeValue or a Failed error.
		 */
		static auto evalHoutExpr(query::Context& ctx, CRef<code::Expr> expr) -> PResult {
			// Try evaluating with TreeEval(Short Path).
			TreeEvalResult tree_eval_result = evaluateWithTreeEval(ctx, expr);
			if (tree_eval_result.hasError()) {
				variant_match(tree_eval_result.error()) {
					variant_case(query::Failed, failed) { return query::QError(query::Failed()); }
					variant_case(CouldNotShortPath, _) {
						// If TreeEval failed, try to evaluate with VM.
						const auto* call_expr = dynamic_cast<const code::CallExpr*>(expr.get());
						if (!call_expr) return query::QError(query::Failed());
						return evaluateFunctionWithVm(ctx, call_expr);
					}
				}
			}
			return tree_eval_result.valueOrThrow();
		}

		static auto provide(query::Context& ctx, const QKey key) -> PResult {
			return evalHoutExpr(ctx, key.expr);
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluateHOUTExpression);

	struct IMPLEMENT_QUERY(QueryEvaluatePSTExpression, CompTimeEvalResult) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto expr = ctx.query<QueryHoutOfExpr>({ key.element });
			if (expr.hasError()) return query::QError(query::Failed());
			return ctx.query<QueryEvaluateHOUTExpression>({ expr.valueOrThrow().ref() });
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluatePSTExpression);

	CompTimeEvalResult getTypeCTVFromPST(
		query::Context& ctx, pst::GenericPSTQueryKey<pst::ExprElement> pst_expr
	) {
		const auto hout_qresult = getHoutOfExprWithExpectedType(
			ctx,
			pst_expr,
			tsh::SymbolType<>{
				ctx.query<tsh::QueryMetaType>({}),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			}
		);
		if (hout_qresult.hasError()) return query::QError(query::Failed());
		return ctx.query<QueryEvaluateHOUTExpression>({ hout_qresult.valueOrThrow().ref() });
	}
}
