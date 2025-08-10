#include "query.hpp"

#include "helios/ctv/ctv.hpp"
#include "helios/helios_errors.hpp"
#include "helios/queries.hpp"
#include "helios_private/comp_time/vm_evaluator.hpp"

#include <backends/dvm/backend.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <pst_parser/elements/includes/basic.hpp>

#include "base/exceptions.hpp"
#include "base/string_id.hpp"
#include "base/variant.hpp"

#include "query_framework/context.hpp"
#include "query_framework/query_cache_macros.hpp"
#include "query_framework/query_result.hpp"
#include <query_framework/query_impl.hpp>

#include "vm/bytecode/bytecode.hpp"

namespace {
	using namespace compiler::helios;

	// TODOP: Checks if the whole subtree is a simple expression.
	struct IsSimpleVisitor final: public code::HoutExprVisitorEmpty {
		bool is_simple = true;

		void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) override {
			if (!is_simple) return;
			expr.lhs->acceptVisitor(*this);
			expr.rhs->acceptVisitor(*this);
		}

		void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) override {
			if (!is_simple) return;
			expr.expr->acceptVisitor(*this);
		}

		void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) override {
			if (!is_simple) return;
			expr.condition->acceptVisitor(*this);
			expr.if_true->acceptVisitor(*this);
			expr.if_false->acceptVisitor(*this);
		}

		void visitParenthesisExpr(const code::ParenthesisExpr& expr) override {
			if (!is_simple) return;
			expr.inner->acceptVisitor(*this);
		}

		void visitSequenceExpr(const code::SequenceExpr& expr) override {
			if (!is_simple) return;
			for (const auto& sub_expr: expr.expressions) {
				sub_expr->acceptVisitor(*this);
				if (!is_simple) return;
			}
		}

		void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) override {
			if (!is_simple) return;
			for (const auto& sub_expr: expr.elements) {
				sub_expr->acceptVisitor(*this);
				if (!is_simple) return;
			}
		}

		void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr) override {
			if (!is_simple) return;
			for (const auto& sub_expr: expr.subtypes) {
				sub_expr->acceptVisitor(*this);
				if (!is_simple) return;
			}
		}

		void visitCallExpr(const code::CallExpr&) override { is_simple = false; }

		void visitAccessExpr(const code::AccessExpr&) override { is_simple = false; }
	};

	struct TreeEvalVisitor final: public code::HoutExprVisitor {
		query::Context&    ctx;
		CompTimeEvalResult result;

		TreeEvalVisitor(query::Context& ctx): ctx(ctx) {}

		static CompTimeEvalResult evaluateWithTreeEval(query::Context& ctx, const code::Expr& expr) {
			TreeEvalVisitor visitor(ctx);
			expr.acceptVisitor(visitor);
			return std::move(visitor.result);
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

		void visitLiteralTypeExpr(const code::LiteralTypeExpr&) final {
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
			return std::move(visitor.result);
		}

		static CompTimeEvalResult evaluateWithVm(query::Context& ctx, const code::Expr& expr) {
			const auto* call_expr = dynamic_cast<const code::CallExpr*>(&expr);
			// TODOP: For now VM is only used for function call evaluation.
			if (!call_expr) return query::QError(errors::Failed());

			// auto pst_function =
			// auto fun_hout_result = ctx.query<helios::QueryModuleHOUT>(const typename
			// OthQuery::QKey &key)
			//j

			auto mir_func_result
				= ctx.query<mir::LowerToMirFunctionResult>({ fun_hout_result.value() });
			if (mir_func_result.hasError()) { /* ... */ }

			auto lir_func_result = ctx.query<lir::LowerToLirFunction>({ mir_func_result.value() });
			if (lir_func_result.hasError()) { /* ... */ }

			// Get code of the called function.
			// TODOP: Maybe add create a backend_vm::Function and don't use Module everywhere?
			backend_vm::Module m{ ctx, base::StrID("COMP_TIME"), { lir_func_result.value() }, {} };
			vm::code::CodeCollection code = m.build();

			std::vector<CTV> ctv_arguments;
			for (const auto& arg_expr: call_expr->arguments) {
				auto arg_result = ctx.query<EvaluateAtCompileTime>({ arg_expr });
				if (arg_result.hasError()) return arg_result;
				ctv_arguments.push_back(std::move(arg_result));
			}
			
			const auto* callee_ident = dynamic_cast<code::IdentifierExpr*>(call_expr->callee);
			auto function_name = callee_ident.symbol.getName();

			return CompileTimeEvaluator::get().executeInVm(
					// Is the compiler return type needed here?
					code,
					function_name,
					ctv_arguments
			);

		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto eval = ctx.query<QueryHoutOfExpr>({ key.element });
			if (eval.hasError()) return query::QError(errors::Failed());

			if (isSimpleEnoughForTreeEval(*eval.value()))
				return evaluateWithTreeEval(ctx, *eval.value());

			evaluateWithVm(ctx, *eval.value());
			return query::QError(errors::Failed());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(EvaluateAtCompileTime);

}
