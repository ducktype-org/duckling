#include "int_eval.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <pst_parser/elements/includes/basic.hpp>

#include <query_framework/query_impl.hpp>

#include <cmath>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(EvalExprToNumCTV, NumCTVEval_Result) {
		struct EvaluateHoutExprVisitor final: public code::HoutExprVisitor {
			Context&                                ctx;
			query::QResult<num_ctv, errors::Failed> result;

			EvaluateHoutExprVisitor(Context& ctx): ctx(ctx) {}

			static query::QResult<num_ctv, errors::Failed> evaluateExpr(
				Context& ctx, const code::Expr& expr
			) {
				EvaluateHoutExprVisitor visitor(ctx);
				expr.acceptVisitor(visitor);
				return visitor.result;
			}

			void visitLiteralNumCTVExpr(const code::LiteralNumCTVExpr& expr) final {
				result = static_cast<const num_ctv&>(expr.value);
			}

			void visitLiteralBoolExpr(const code::LiteralBoolExpr&) final {
				throw base::NotYetImplemented("Evaluation of boolean values is not implemented yet");
			}

			void visitLiteralStringExpr(const code::LiteralStringExpr&) final {
				throw base::NotYetImplemented("Evaluation of string values is not implemented yet");
			}

			void visitCallExpr(const code::CallExpr&) final {
				throw base::NotYetImplemented("Evaluation of calls");
			}

			void visitLiteralTypeExpr(const code::LiteralTypeExpr&) final {
				throw base::NotYetImplemented("Evaluation of type values is not implemented yet");
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
				result = ctx.query<QueryConstValueOf>(expr.symbol);
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
				auto lhs_result = evaluateExpr(ctx, *expr.lhs);
				if (lhs_result.hasError()) {
					result = lhs_result;
					return;
				}

				auto rhs_result = evaluateExpr(ctx, *expr.rhs);
				if (rhs_result.hasError()) {
					result = rhs_result;
					return;
				}

				const auto& lhs_value = lhs_result.value();
				const auto& rhs_value = rhs_result.value();

				result = std::visit(
					[&](auto lhs, auto rhs) -> num_ctv {
						using LhsT = decltype(lhs);
						using RhsT = decltype(rhs);

						// types potentially to big to prevent overflow
						using ResultT = std::conditional_t<
							std::is_integral_v<LhsT> && std::is_integral_v<RhsT>,
							i64,
							f80>;

						ResultT result_value;

						if constexpr (std::is_arithmetic_v<ResultT>) {
							switch (expr.operation) {
							case code::BuiltinBinary::IntegerAdd:
								result_value
									= static_cast<ResultT>(lhs) + static_cast<ResultT>(rhs);
								break;

							case code::BuiltinBinary::IntegerSub:
								result_value
									= static_cast<ResultT>(lhs) - static_cast<ResultT>(rhs);
								break;

							case code::BuiltinBinary::IntegerMul:
								result_value
									= static_cast<ResultT>(lhs) * static_cast<ResultT>(rhs);
								break;
							case code::BuiltinBinary::IntegerDiv:
								result_value
									= static_cast<ResultT>(lhs) / static_cast<ResultT>(rhs);
								break;
							case code::BuiltinBinary::IntegerMod:
								result_value = static_cast<i64>(lhs) % static_cast<i64>(rhs);
								break;
							case code::BuiltinBinary::IntegerPow:
								result_value = static_cast<ResultT>(std::pow(lhs, rhs));
								break;
							case code::BuiltinBinary::BooleanAnd:
								return static_cast<num_ctv>(
									static_cast<bool>(lhs) && static_cast<bool>(rhs)
								);
							case code::BuiltinBinary::BooleanOr:
								return static_cast<num_ctv>(
									static_cast<bool>(lhs) || static_cast<bool>(rhs)
								);
							default:
								throw base::NotYetImplemented(
									"Evaluation of other operators is not implemented yet"
								);
							}
						} else {
							throw std::runtime_error("Operands must be arithmetic");
						}
						return make_minimized_num_ctv(result_value);
					},
					lhs_value,
					rhs_value
				);
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
				result = evaluateExpr(ctx, *expr.expr);
				if (result.hasError()) return;

				const num_ctv& result_value = result.value();

				switch (expr.operation) {
				case code::BuiltinUnary::IntegerNegation:
					result = std::visit(
						[](auto val) -> NumCTVEval_Result {
							using T = decltype(val);
							if constexpr (std::is_arithmetic_v<T>)
								return static_cast<num_ctv>(-val);  // Safe unary minus
							else
								return query::QError(errors::Failed());
						},
						result_value
					);
					break;
				default:
					result = query::QError(errors::Failed());
					break;
				}
			}

			void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) final {
				auto cond_result = evaluateExpr(ctx, *expr.condition);
				if (cond_result.hasError()) {
					result = cond_result;
					return;
				}
				auto true_result = evaluateExpr(ctx, *expr.if_true);
				if (true_result.hasError()) {
					result = true_result;
					return;
				}
				auto false_result = evaluateExpr(ctx, *expr.if_false);
				if (false_result.hasError()) {
					result = false_result;
					return;
				}

				result = std::visit(
					[&](auto cond_val) -> NumCTVEval_Result {
						using T = decltype(cond_val);
						if constexpr (std::is_arithmetic_v<T>) {
							bool condition = static_cast<bool>(cond_val);
							return condition ? true_result : false_result;
						} else {
							return query::QError(errors::Failed());
						}
					},
					cond_result.value()
				);
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
				result = evaluateExpr(ctx, *expr.inner);
			}

			void visitTupleTypeConstructorExpr(
				[[maybe_unused]] const code::TupleTypeConstructorExpr& expr
			) final {
				throw base::NotYetImplemented("Evaluation of tuple values is not implemented yet");
			}

			void visitVariantTypeConstructorExpr(
				[[maybe_unused]] const code::VariantTypeConstructorExpr& expr
			) final {
				throw base::NotYetImplemented("Evaluation of variant values is not implemented yet");
			}

			void visitAccessExpr(const code::AccessExpr&) final {
				throw base::NotYetImplemented(
					"Evaluation of access expressions is not implemented yet"
				);
			}

			void visitSequenceExpr(const code::SequenceExpr& seq) final {
				result = evaluateExpr(ctx, *seq.expressions.back());
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto eval = ctx.query<QueryHoutOfExpr>({ key.element });
			if (eval.hasError()) return query::QError(errors::Failed());
			return EvaluateHoutExprVisitor::evaluateExpr(ctx, *eval.value());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(EvalExprToNumCTV);
};
