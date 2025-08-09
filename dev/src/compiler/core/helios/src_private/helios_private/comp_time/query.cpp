#include "query.hpp"

#include "helios/ctv/ctv.hpp"
#include "helios/helios_errors.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <pst_parser/elements/includes/basic.hpp>

#include "base/exceptions.hpp"
#include "base/variant.hpp"

#include "query_framework/context.hpp"
#include "query_framework/query_cache_macros.hpp"
#include <query_framework/query_impl.hpp>

namespace {
	using namespace compiler::helios;

	// TODOP: Checks if the whole subtree is a simple expression.
	struct IsSimpleVisitor final: public code::HoutExprVisitor {
		bool is_simple = true;

		void visitCallExpr(const code::CallExpr&) final { is_simple = false; }

		void visitAccessExpr(const code::AccessExpr&) final { is_simple = false; }

		void defaultVisit(const code::Expr& expr) final {
			if (!is_simple) return;
			compiler::helios::code::HoutExprVisitor::defaultVisit(expr);
		}
	};

	struct TreeEvalVisitor final: public code::HoutExprVisitor {
		query::Context&    ctx;
		CompTimeEvalResult result;

		TreeEvalVisitor(query::Context& ctx): ctx(ctx) {}

		static CompTimeEvalResult evaluateWithTreeEval(query::Context& ctx, const code::Expr& expr) {
			TreeEvalVisitor visitor(ctx);
			expr.acceptVisitor(visitor);
			return visitor.result;
		}

		// TODOP: Triage if the expression is simple enough for tree eval.
		bool isSimpleEnoughForTreeEval(const code::Expr& expr) {
			IsSimpleVisitor visitor;
			expr.acceptVisitor(visitor);
			return visitor.is_simple;
		}

		void visitLiteralIntExpr(const code::LiteralIntExpr& expr) final {
			result = CompileTimeValue{ expr.value };
		}

		void visitLiteralBoolExpr(const code::LiteralBoolExpr& expr) final {
			result = CompileTimeValue{ expr.value };
		}

		void visitLiteralStringExpr(const code::LiteralStringExpr&) final {
			throw base::NotYetImplemented("Evaluation of string values is not implemented yet");
		}

		void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) final {
			throw base::NotYetImplemented("Evaluation of type values is not implemented yet");
			// result = CompileTimeValue{ expr.value_type };
		}

		void visitCallExpr(const code::CallExpr&) final {
			throw base::NotYetImplemented("Evaluation of calls");
		}

		void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
			result = ctx.query<QueryConstValueOf>(expr.symbol);
			// result = ctx.query<EvaluateAtCompileTime>({ expr.symbol_data->definition });
		}

		void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
			auto lhs_result = ctx.query<compiler::helios::EvaluateAtCompileTime>({ *expr.lhs });
			if (lhs_result.hasError()) {
				result = lhs_result;
				return;
			}

			auto rhs_result = ctx.query<compiler::helios::EvaluateAtCompileTime>({ *expr.rhs });
			if (rhs_result.hasError()) {
				result = rhs_result;
				return;
			}

			const auto& lhs_ctv = lhs_result.value();
			const auto& rhs_ctv = rhs_result.value();

			// TODOP: This is freaking goofy with those n^2 cases. Think of a better way.
			variant_match(lhs_ctv) {
				variant_case(i64, lhs_value) {
					variant_match(rhs_ctv) {
						variant_case(i64, rhs_value) {
							switch (expr.operation) {
							case code::BuiltinBinary::IntegerAdd:
								result = CTV{ lhs_value + rhs_value };
								break;
							case code::BuiltinBinary::IntegerSub:
								result = CTV{ lhs_value - rhs_value };
								break;
							case code::BuiltinBinary::IntegerMul:
								result = CTV{ lhs_value * rhs_value };
								break;
							case code::BuiltinBinary::IntegerDiv:
								result = CTV{ lhs_value / rhs_value };
								break;
							case code::BuiltinBinary::IntegerMod:
								result = CTV{ lhs_value % rhs_value };
								break;
							case code::BuiltinBinary::IntegerPow:
								result = CTV{ static_cast<i64>(std::pow(lhs_value, rhs_value)) };
								break;
							case code::BuiltinBinary::BooleanAnd:
								result = CTV{ lhs_value and rhs_value };
								break;
							case code::BuiltinBinary::BooleanOr:
								result = CTV{ lhs_value or rhs_value };
								break;
							default:
								result = query::QError(errors::Failed());
								throw base::NotYetImplemented(
									"Evaluation of different than '+-*/%**' binary operators "
									"is "
									"not "
									"implemented yet"
								);
							}
						}
						variant_default {
							// Type Mismatch.
							result = query::QError(errors::Failed());
						}
					}
				}
				variant_case(bool, lhs_value) {
					variant_match(rhs_ctv) {
						variant_case(bool, rhs_value) {
							switch (expr.operation) {
							case code::BuiltinBinary::BooleanAnd:
								result = CTV{ lhs_value and rhs_value };
								break;
							case code::BuiltinBinary::BooleanOr:
								result = CTV{ lhs_value or rhs_value };
								break;
							default:
								result = query::QError(errors::Failed());
								throw base::NotYetImplemented(
									"Evaluation of different than '&&, ||' binary operators "
									"for "
									"booleans is "
									"not "
									"implemented yet"
								);
							}
						}
					}
				}
				variant_case(const VmHeldValue&, lhs_value) {
					result = query::QError(errors::Failed());
					CORE_PANIC("Tree eval encountered a VM-held value. This should not happen");
				}
				variant_default { result = query::QError(compiler::helios::errors::Failed()); }
			}
		}

		void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
			auto expr_result = ctx.query<compiler::helios::EvaluateAtCompileTime>({ *expr.expr });

			if (expr_result.hasError()) {
				result = expr_result;
				return;
			}

			const auto& ctv = expr_result.value();

			variant_match(ctv) {
				variant_case(i64, val) {
					switch (expr.operation) {
					case code::BuiltinUnary::IntegerNegation:
						result = CTV{ -val };
						break;
					default:
						result = query::QError(errors::Failed());
						break;
					}
				}
				variant_case(bool, val) {
					switch (expr.operation) {
					case code::BuiltinUnary::BooleanNot:
						result = CTV{ not val };
						break;
					default:
						result = query::QError(errors::Failed());
						break;
					}
				}
				variant_case(tsh::SymbolType<>, type_val) {
					switch (expr.operation) {
					case code::BuiltinUnary::Ref:
						result = CTV{ type_val.withReferenceKind(tsh::ReferenceKind::Ref) };
						break;
					case code::BuiltinUnary::Box:
						result = CTV{ type_val.withReferenceKind(tsh::ReferenceKind::Box) };
						break;
					default:
						result = query::QError(errors::Failed());
						break;
					}
				}
				variant_default {
					result = query::QError(compiler::helios::errors::Failed());
					throw base::NotYetImplemented(
						"Unary operators for other types are not implemented"
					);
				}
			}
		}

		void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) final {
			auto cond_result
				= ctx.query<compiler::helios::EvaluateAtCompileTime>({ *expr.condition });
			if (cond_result.hasError()) {
				result = cond_result;
				return;
			}

			bool        condition_is_true = false;
			const auto& cond_ctv          = cond_result.value();
			variant_match(cond_ctv) {
				variant_case(bool, val) { condition_is_true = val; }
				variant_case(i64, val) { condition_is_true = (val != 0); }
				variant_default {
					result = query::QError(compiler::helios::errors::Failed());
					return;
				}
			}

			if (condition_is_true)
				result = ctx.query<compiler::helios::EvaluateAtCompileTime>({ expr.if_true });
			else
				result = ctx.query<compiler::helios::EvaluateAtCompileTime>({ expr.if_false });
		}

		void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
			result = ctx.query<compiler::helios::EvaluateAtCompileTime>({ *expr.inner });
		}

		void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr&) final {
			throw base::NotYetImplemented("Evaluation of tuple values is not implemented yet");
		}

		void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr&) final {
			throw base::NotYetImplemented("Evaluation of variant values is not implemented yet");
		}

		void visitAccessExpr(const code::AccessExpr&) final {
			throw base::NotYetImplemented("Evaluation of access expressions is not implemented yet");
		}

		void visitSequenceExpr(const code::SequenceExpr& seq) final {
			result
				= ctx.query<compiler::helios::EvaluateAtCompileTime>({ *seq.expressions.back() });
		}
	};

}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(EvaluateAtCompileTime, CompTimeEvalResult) {
		static bool isSimpleEnoughForTreeEval(const code::Expr& expr) {
			IsSimpleVisitor visitor;
			expr.acceptVisitor(visitor);
			return visitor.is_simple;
		}

		static CompTimeEvalResult evaluateWithTreeEval(query::Context& ctx, const code::Expr& expr) {
			TreeEvalVisitor visitor(ctx);
			expr.acceptVisitor(visitor);
			return visitor.result;
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto eval = ctx.query<QueryHoutOfExpr>({ key.element });
			if (eval.hasError()) return query::QError(errors::Failed());

			if (isSimpleEnoughForTreeEval(*hout.value()))
				return evaluateWithTreeEval(ctx, *hout.value());

			// TODOP: Add VM eval here
			return query::QError(errors::Failed());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(EvaluateAtCompileTime);

}
