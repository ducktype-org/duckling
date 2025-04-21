#include "int_eval.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios_private/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <query_framework/query_impl.hpp>

#include <cmath>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(EvalExprToI64, IntEval_Result) {
		struct EvaluateHoutExprVisitor final: public code::HoutExprVisitor {
			Context&                                  ctx;
			errors::HResult<i64, errors::Failed> result;

			EvaluateHoutExprVisitor(Context& ctx): ctx(ctx) {}

			static errors::HResult<i64, errors::Failed> evaluateExpr(
				Context& ctx, const code::Expr& expr
			) {
				EvaluateHoutExprVisitor visitor(ctx);
				expr.acceptVisitor(visitor);
				return visitor.result;
			}

			void visitLiteralIntExpr(const code::LiteralIntExpr& expr) final {
				result = expr.value;
			}

			void visitLiteralBoolExpr(const code::LiteralBoolExpr&) final {
				throw base::NotYetImplemented("Evaluation of boolean values is not implemented yet");
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
				i64 lhs_value = lhs_result.value(), rhs_value = rhs_result.value();
				switch (expr.operation) {
				case code::BuiltinBinary::IntegerAdd:
					result = lhs_value + rhs_value;
					break;
				case code::BuiltinBinary::IntegerSub:
					result = lhs_value - rhs_value;
					break;
				case code::BuiltinBinary::IntegerMul:
					result = lhs_value * rhs_value;
					break;
				case code::BuiltinBinary::IntegerDiv:
					result = lhs_value / rhs_value;
					break;
				case code::BuiltinBinary::IntegerMod:
					result = lhs_value % rhs_value;
					break;
				case code::BuiltinBinary::IntegerPow:
					result = std::pow(lhs_value, rhs_value);
					break;
				default:
					result = errors::HError(errors::Failed());
					throw base::NotYetImplemented(
						"Evaluation of different than '+-*/%**' binary operators is not "
						"implemented yet"
					);
				}
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
				result = evaluateExpr(ctx, *expr.expr);
				if (result.hasError()) return;
				i64 result_value = result.value();

				switch (expr.operation) {
				case code::BuiltinUnary::IntegerNegation:
					result = -result_value;
					break;
				default:
					result = errors::HError(errors::Failed());
					break;
				}
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

			void visitLinkedIdentifierExpr(const code::LinkedIdentifierExpr& expr) final {
				result = ctx.query<QueryConstValueOf>(expr.symbols.back());
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto eval = ctx.query<QueryHoutOfExpr>({ key.element });
			if (eval.hasError()) return errors::HError(errors::Failed());
			return EvaluateHoutExprVisitor::evaluateExpr(ctx, *eval.value());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(EvalExprToI64);
};
