#include "comp_time.hpp"

#include "ctv/ctv.hpp"
#include "ctv/numeric_value.hpp"
#include "helios/helios_errors.hpp"
#include "helios/hout/elements/expr.hpp"

#include <backends/dvm/backend.hpp>
#include <ctv/ctv.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios_private/comp_time/vm_evaluator.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <typesystem/higher/queries/types.hpp>

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"

#include "query_framework/query_result.hpp"
#include <query_framework/context.hpp>
#include <query_framework/query_impl.hpp>

#include <vm/bytecode/bytecode.hpp>

#include <cmath>
#include <ranges>
#include <type_traits>
#include <variant>

static int counter = 0;

#define DEBUG(CONTENT) \
	std::cout << "[QUERY EVALUATE EXPRESSION " << counter << "]: " << CONTENT << '\n';

namespace compiler::helios {
	using namespace ctv;

	struct IMPLEMENT_QUERY(QueryEvaluateExpression, CompTimeEvalResult) {
		/**
		 * @brief Error indicating that an expression was to complex for a simple tree evaluation.
		 */
		struct CouldNotShortPath {};

		using TreeEvalResult = query::QResult<CompileTimeValue, CouldNotShortPath, errors::Failed>;

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
				DEBUG("Literal Unit");
				result = CompileTimeValue{ CompileTimeValue::UnitCTV{} };
			}

			void visitLiteralNumericExpr(const code::LiteralNumericExpr& expr) final {
				DEBUG("Literal Numeric");
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralBoolExpr(const code::LiteralBoolExpr& expr) final {
				DEBUG("Literal Bool");
				result = CompileTimeValue{ expr.value };
			}

			void visitLiteralStringExpr(const code::LiteralStringExpr&) final {
				DEBUG("Literal String");
				throw base::NotYetImplemented("Evaluation of string values in compile time");
			}

			void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) final {
				DEBUG("Literal Type");
				result = CompileTimeValue{ expr.value_type };
			}

			void visitCallExpr(const code::CallExpr&) final {
				DEBUG("Call expr");
				result = query::QError(CouldNotShortPath{});
			}

			void visitAccessExpr(const code::AccessExpr&) final {
				DEBUG("Access Expr");
				result = query::QError(CouldNotShortPath{});
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
				DEBUG("Identifier expr");
				// Type Evaluation.
				if (expr.expression_type.getType().getKind() == tsh::Kind::Meta) {
					auto type = ctx.query<QueryTypeFromDefinition>({ expr.symbol });
					result    = type->hasValue()
					              ? CompTimeEvalResult{ CompileTimeValue{ type->value() } }
					              : query::QError(errors::Failed());
				} else {
					// Constant Evaluation.
					auto const_val_result = ctx.query<QueryConstValueOf>({ expr.symbol });
					result                = const_val_result.hasValue()
					                          ? CompTimeEvalResult{ const_val_result.value() }
					                          : query::QError(errors::Failed());
				}
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
				DEBUG("Binary operator");
				auto lhs_result = evalHoutExpr(ctx, expr.lhs.ref());
				if (lhs_result.hasError()) {
					result = query::QError(errors::Failed());
					return;
				}

				auto rhs_result = evalHoutExpr(ctx, expr.rhs.ref());
				if (rhs_result.hasError()) {
					result = query::QError(errors::Failed());
					return;
				}

				/// 123 + 123;

				const auto& lhs_ctv = lhs_result.value();
				const auto& rhs_ctv = rhs_result.value();

				result = std::visit(
					[&](auto&& lhs, auto&& rhs) -> TreeEvalResult {
						using LhsT = std::decay_t<decltype(lhs)>;
						using RhsT = std::decay_t<decltype(rhs)>;

						// Binary operation on  numeric literals.
						if constexpr (std::is_same_v<LhsT, NumericValue>
					                  && std::is_same_v<RhsT, NumericValue>) {
							return std::visit(
								[&](auto&& lhs_val, auto&& rhs_val) -> TreeEvalResult {
									using LhsNumT = std::decay_t<decltype(lhs_val)>;
									using RhsNumT = std::decay_t<decltype(rhs_val)>;
									// Find a common type for those literals.
									using CommonTypeT = std::common_type_t<LhsNumT, RhsNumT>;

									auto maybe_lhs_coerced = lhs.template coerceTo<CommonTypeT>();
									auto maybe_rhs_coerced = rhs.template coerceTo<CommonTypeT>();

									// Failed to coerce.
									if (!maybe_lhs_coerced || !maybe_rhs_coerced)
										return query::QError(errors::Failed());
									auto lhs_coerced = maybe_lhs_coerced.value();
									auto rhs_coerced = maybe_rhs_coerced.value();

									// TODOP: Check for over/underflows?
									using enum code::BuiltinBinary;
									switch (expr.operation) {
									case IntegerAdd:
									case FloatAdd:
										return CompileTimeValue{ NumericValue{ lhs_coerced
									                                           + rhs_coerced } };
									case IntegerSub:
									case FloatSub:
										return CompileTimeValue{ NumericValue{ lhs_coerced
									                                           - rhs_coerced } };
									case IntegerMul:
									case FloatMul:
										return CompileTimeValue{ NumericValue{ lhs_coerced
									                                           * rhs_coerced } };
									case IntegerDiv:
									case FloatDiv:
										if (rhs_coerced == 0)
											return query::QError(errors::Failed());
										return CompileTimeValue{ NumericValue{ lhs_coerced
									                                           / rhs_coerced } };
									case IntegerMod:
									case FloatMod:
										if (rhs_coerced == 0)
											return query::QError(errors::Failed());
										if constexpr (std::is_integral_v<CommonTypeT>) {
											return CompileTimeValue{ NumericValue{
												lhs_coerced % rhs_coerced } };
										} else {
											return CompileTimeValue{ NumericValue{
												std::fmod(lhs_coerced, rhs_coerced) } };
										}
									case IntegerPow:
									case FloatPow:
										return CompileTimeValue{ NumericValue{
											static_cast<CommonTypeT>(
												std::pow(lhs_coerced, rhs_coerced)
											) } };
									default:
										throw base::NotYetImplemented(
											"Evaluation of other binary operators in compile time"
										);
									}
								},
								lhs.getStorage(),
								rhs.getStorage()
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
							return query::QError(errors::Failed());
						}
					},
					lhs_ctv.getStorage(),
					rhs_ctv.getStorage()
				);
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
				DEBUG("Unary operator");
				auto expr_result = evalHoutExpr(ctx, expr.expr.ref());
				if (expr_result.hasError()) {
					result = query::QError(errors::Failed(expr_result.error()));
					return;
				}

				const auto& ctv = expr_result.value();


				result = std::visit(
					[&](auto&& val) -> TreeEvalResult {
						using T = std::decay_t<decltype(val)>;
						using enum code::BuiltinUnary;

						if constexpr (std::is_same_v<T, NumericValue>) {
							switch (expr.operation) {
							case IntegerNegation:
							case FloatNegation:
								return std::visit(
									[&](auto&& num_val) {
										using NumT = std::decay<decltype(num_val)>;
										if constexpr (std::is_unsigned_v<NumT>)
											return query::QError(errors::Failed());
										else
											return CompileTimeValue{ NumericValue{ -num_val } };
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
								return query::QError(errors::Failed());
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
				DEBUG("Ternary operator");
				auto cond_result = evalHoutExpr(ctx, expr.condition.ref());
				if (cond_result.hasError()) {
					result = query::QError(errors::Failed(cond_result.error()));
					return;
				}

				bool        condition_is_true = false;
				const auto& cond_ctv          = cond_result.value();
				variant_match(cond_ctv.getStorage()) {
					variant_case(bool, val) { condition_is_true = val; }
					variant_case(NumericValue, val) {
						condition_is_true = std::visit(
							[&](auto&& num_val) { return (num_val != 0); }, val.getStorage()
						);
					}
					variant_default {
						result = query::QError(errors::Failed());
						return;
					}
				}

				if (condition_is_true)
					result = evalHoutExpr(ctx, expr.if_true.ref());
				else
					result = evalHoutExpr(ctx, expr.if_false.ref());
			}

			void visitChainComparisonExpr(const code::ChainComparisonExpr& chain_expr) final {
				DEBUG("Comparison chain");
				auto compare = [](const NumericValue& first,
				                  const NumericValue& second,
				                  code::BuiltinBinary operation) {
					return std::visit(
						[&](auto&& lhs, auto&& rhs) -> bool {
							using LhsNumT = std::decay_t<decltype(lhs)>;
							using RhsNumT = std::decay_t<decltype(rhs)>;
							// Find a common type for those literals.
							using CommonTypeT = std::common_type_t<LhsNumT, RhsNumT>;

							auto lhs_num = static_cast<CommonTypeT>(lhs);
							auto rhs_num = static_cast<CommonTypeT>(rhs);

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
					result = query::QError(errors::Failed(evaluated.error()));
					return;
				}
				auto prev_value = evaluated.value();
				for (auto [next_expr, comp]: zip(evaluated_exprs | drop(1), chain_expr.operators)) {
					if (next_expr.hasError()) {
						result = query::QError(errors::Failed(evaluated.error()));
						return;
					}

					auto next_value = next_expr.value();
					if (!compare(
							*prev_value.get<NumericValue>(), *next_value.get<NumericValue>(), comp
						)) {
						result = CompileTimeValue{ false };
						return;
					}

					prev_value = next_value;
				}
				result = CompileTimeValue{ true };
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
				DEBUG("Parenthesis expr");
				result = evalHoutExpr(ctx, expr.inner.ref());
			}

			void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) final {
				DEBUG("Tuple constructor");
				std::vector<tsh::SymbolType<>> subtypes;

				for (auto& sub_type: expr.elements) {
					auto sub_type_result = evalHoutExpr(ctx, sub_type.ref());
					if (sub_type_result.hasError()) {
						result = query::QError(errors::Failed(sub_type_result.error()));
						return;
					}

					variant_match(sub_type_result.value().getStorage()) {
						variant_case(tsh::SymbolType<>, type) { subtypes.emplace_back(type); }
						variant_default { CORE_PANIC("Type evaluation returned not a type\n"); }
					}
				}

				result = CompileTimeValue{ tsh::SymbolType<>{
					ctx.query<tsh::QueryTupleType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				} };
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) final {
				DEBUG("Variant constructor");
				std::vector<tsh::SymbolType<>> subtypes;
				for (auto& sub_type: expr.subtypes) {
					// should we here short-path or not?
					auto sub_type_result = evalHoutExpr(ctx, sub_type.ref());
					if (sub_type_result.hasError()) {
						result = query::QError(errors::Failed(sub_type_result.error()));
						return;
					}

					match_optional(sub_type_result.value().getType(ctx)) {
						opt_some(sub_type) { subtypes.emplace_back(sub_type); }
						opt_none { CORE_PANIC("Type evaluation returned not a type\n"); }
					}
				}

				result = CompileTimeValue{ tsh::SymbolType<>{
					ctx.query<tsh::QueryVariantType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				} };
			}

			void visitSequenceExpr(const code::SequenceExpr& seq) final {
				DEBUG("Sequence constructor");
				result = evalHoutExpr(ctx, seq.expressions.back().ref());
			}
		};

		/**
		 * @brief Evaluates a HOUT call expression using VM Eval.
		 * @return The calculated result represented by CompileTimeValue or a Failed error.
		 */
		static CompTimeEvalResult evaluateFunctionWithVm(query::Context& ctx, CRef<code::Expr> expr) {
			DEBUG("Evaluate function with VM");
			// @todo: For now this works only with functions which don't call any other functions.
			// This should change in #1203
			using namespace compiler;

			const auto* call_expr = dynamic_cast<const code::CallExpr*>(expr.get());
			if (!call_expr) return query::QError(errors::Failed());

			const auto* callee_ident
				= dynamic_cast<const code::IdentifierExpr*>(call_expr->callee.operator->());
			if (!callee_ident) return query::QError(errors::Failed());

			const SymID function_sym_id = callee_ident->symbol;

			// Get code of the called function.
			// @todo: Change this code to a single query once it gets implemented #826.
			auto fun_hout_result = ctx.query<QueryCodeOfFun>(function_sym_id);

			auto mir_func_result = ctx.query<mir::LowerToMirFunction>({ fun_hout_result });
			if (mir_func_result->hasError()) return query::QError(mir_func_result->error());

			CRef<mir::Function> mir_func        = &mir_func_result->value();
			auto                lir_func_result = ctx.query<lir::LowerToLirFunction>({ mir_func });

			backend_vm::Module       m{ ctx, base::StrID("COMP_TIME"), { lir_func_result }, {} };
			vm::code::CodeCollection code = m.build();

			std::vector<CompileTimeValue> ctv_arguments;
			for (const auto& arg_expr: call_expr->arguments) {
				auto arg_result = evalHoutExpr(ctx, arg_expr.ref());
				if (arg_result.hasError()) return arg_result;
				ctv_arguments.push_back(arg_result.value());
			}

			// Retrieve the functions return type.
			auto callee_abs_type = callee_ident->expression_type.getSymbolType().getType();
			if (callee_abs_type.getKind() != tsh::Kind::Function) {
				CORE_PANIC(
					"Attempting to call a non_function type during VM compile time evaluation"
				);
			}
			tsh::FunctionAbstractType func_type(callee_abs_type);

			auto vm_eval_result = executeInVm(
				lir_func_result->mangled_name.str(), code, ctv_arguments, func_type.getResultType()
			);

			if (!vm_eval_result) return query::QError(errors::Failed());
			return vm_eval_result.value();
		}

		/**
		 * @brief Evaluates a HOUT expression using TreeEval.
		 * @return The calculated result represented by CompileTimeValue, a CouldNotShortPath error
		 * if the expresion was to complicated for tree eval or a Failed error.
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
					variant_case(errors::Failed, failed) {
						DEBUG("Eval with tree eval has error");

						return query::QError(errors::Failed());
					}
					variant_case(CouldNotShortPath, _) {
						// If TreeEval failed, try to evaluate with VM.
						DEBUG("Eval with tree eval could not shortpath");
						return evaluateFunctionWithVm(ctx, expr);
					}
				}
			}
			DEBUG("Success!");
			return tree_eval_result.value();
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			counter++;
			DEBUG("Called for:");
			key.element.unlock(ctx)->debugPrint(std::cout);
			std::cout << '\n';
			DEBUG("==================");
			auto expr = ctx.query<QueryHoutOfExpr>({ key.element });
			if (expr.hasError()) {
				counter--;
				DEBUG("Query hout of expr is error");
				return query::QError(errors::Failed());
			}
			auto res = evalHoutExpr(ctx, expr.value().ref());
			counter--;
			return res;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluateExpression);
}
