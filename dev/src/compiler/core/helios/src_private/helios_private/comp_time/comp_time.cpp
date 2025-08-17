#include "comp_time.hpp"

#include "helios/ctv/ctv.hpp"
#include "helios/helios_errors.hpp"
#include "helios/queries.hpp"
#include "helios/symbols/query_type_from_definition.hpp"
#include "helios_private/comp_time/vm_evaluator.hpp"
#include "mir/mir_structure/mir_structure.hpp"
#include "typesystem/higher/queries/types.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <backends/dvm/backend.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
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

#include <cmath>

namespace {
	using namespace compiler::helios;

	/**
	 * @brief Representing a compile time evaluation which couldn't be evaluated with tree eval.
	 */
	// TODOP: Use that.
	struct CouldNotShortPath {};

	// TODOP: Checks if the whole subtree is a simple expression.
	// TODOP: Remove that.
	struct IsSimpleVisitor final: public code::HoutExprVisitorEmpty {
		bool is_simple = true;

		void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) override {
			std::cout << "visitBinaryOperatorExpr\n";
			if (!is_simple) return;
			expr.lhs->acceptVisitor(*this);
			expr.rhs->acceptVisitor(*this);
		}

		void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) override {
			std::cout << "visitUnaryOperatorExpr\n";
			if (!is_simple) return;
			expr.expr->acceptVisitor(*this);
		}

		void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) override {
			std::cout << "visitTernaryOperatorExpr\n";
			if (!is_simple) return;
			expr.condition->acceptVisitor(*this);
			expr.if_true->acceptVisitor(*this);
			expr.if_false->acceptVisitor(*this);
		}

		void visitParenthesisExpr(const code::ParenthesisExpr& expr) override {
			std::cout << "visitParenthesisExpr\n";
			if (!is_simple) return;
			expr.inner->acceptVisitor(*this);
		}

		void visitSequenceExpr(const code::SequenceExpr& expr) override {
			std::cout << "visitSequenceExpr\n";
			if (!is_simple) return;
			for (const auto& sub_expr: expr.expressions) {
				sub_expr->acceptVisitor(*this);
				if (!is_simple) return;
			}
		}

		void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) override {
			std::cout << "visitTupleTypeConstructorExpr\n";
			if (!is_simple) return;
			for (const auto& sub_expr: expr.elements) {
				sub_expr->acceptVisitor(*this);
				if (!is_simple) return;
			}
		}

		void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr) override {
			std::cout << "visitVariantTypeConstructorExpr\n";
			if (!is_simple) return;
			for (const auto& sub_expr: expr.subtypes) {
				sub_expr->acceptVisitor(*this);
				if (!is_simple) return;
			}
		}

		void visitCallExpr(const code::CallExpr&) override {
			is_simple = false;
			std::cout << "visitCallExpr\n";
		}

		void visitAccessExpr(const code::AccessExpr&) override {
			is_simple = false;
			std::cout << "visitAccessExpr\n";
		}
	};

	// TODOP: Hout walker
	struct TreeEvalVisitor final: public code::HoutExprVisitor {
		query::Context&    ctx;
		CompTimeEvalResult result;

		TreeEvalVisitor(query::Context& ctx): ctx(ctx) {}

		void visitLiteralIntExpr(const code::LiteralIntExpr& expr) final {
			std::cout << "LiteralIntExpr\n";
			result = CompileTimeValue{ expr.value };
		}

		void visitLiteralBoolExpr(const code::LiteralBoolExpr& expr) final {
			std::cout << "visitLiteralBoolExpr\n";
			result = CompileTimeValue{ expr.value };
		}

		void visitLiteralStringExpr(const code::LiteralStringExpr&) final {
			std::cout << "visitLiteralStringExpr\n";
			throw base::NotYetImplemented("Evaluation of string values in compile time");
		}

		void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) final {
			std::cout << "visitLiteralTypeExpr\n";
			result = CompileTimeValue{ expr.value_type };
		}

		void visitCallExpr(const code::CallExpr&) final {
			std::cout << "visitCallExpr\n";
			// TODOP: return not able to short path or sth.
			result = query::QError(errors::Failed());
			throw base::NotYetImplemented("Tree Evaluation of call expressions");
		}

		void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
			std::cout << "visitIdentifierExpr\n";
			// Type Evaluation.
			if (expr.expression_type.getType().getKind() == tsh::Kind::Meta) {
				auto type = ctx.query<QueryTypeFromDefinition>({ expr.symbol });
				if (type->hasValue())
					result = CTV{ type->value() };
				else
					result = query::QError(errors::Failed());
			} else {
				// Constant Evaluation.
				result = ctx.query<QueryConstValueOf>({ expr.symbol });
			}
		}

		void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
			std::cout << "visitBinaryOperatorExpr\n";
			auto lhs_result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
				{ expr.lhs.operator->() }
			);
			if (lhs_result.hasError()) {
				result = lhs_result;
				return;
			}

			auto rhs_result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
				{ expr.rhs.operator->() }
			);
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
				// variant_case(const VmHeldValue&, lhs_value) {
				// 	result = query::QError(errors::Failed());
				// 	CORE_PANIC("Tree eval encountered a VM-held value. This should not happen");
				// }
				variant_default { result = query::QError(compiler::helios::errors::Failed()); }
			}
		}

		void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
			std::cout << "visitUnaryOperatorExpr\n";
			auto expr_result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
				{ expr.expr.operator->() }
			);

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
						CORE_PANIC(
							"TreeEvalVisitor encountered unsupported unary operation: ",
							static_cast<std::uint8_t>(expr.operation)
						);
					}
				}
				variant_default {
					result = query::QError(compiler::helios::errors::Failed());
					throw base::NotYetImplemented("Evaluation of unary operators for other types.");
				}
			}
		}

		void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) final {
			std::cout << "visitTernaryOperatorExpr\n";
			auto cond_result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
				{ expr.condition.operator->() }
			);
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
				result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
					{ expr.if_true.operator->() }
				);
			else
				result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
					{ expr.if_false.operator->() }
				);
		}

		void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
			std::cout << "visitParenthesisExpr\n";
			result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
				{ expr.inner.operator->() }
			);
		}

		void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) final {
			std::cout << "visitTupleTypeConstructorExpr\n";
			std::vector<tsh::SymbolType<>> subtypes;

			for (auto& sub_type: expr.elements) {
				auto sub_type_result
					= ctx.query<QueryEvaluateHoutExpressionCT>({ sub_type.operator->() });
				if (sub_type_result.hasError()) {
					result = query::QError(errors::Failed(sub_type_result.error()));
					return;
				}

				variant_match(sub_type_result.value()) {
					variant_case(tsh::SymbolType<>, type) { subtypes.emplace_back(type); }
					variant_default { CORE_PANIC("Type evaluation returned not a type\n"); }
				}
			}

			result = CTV{ tsh::SymbolType<>{
				ctx.query<tsh::QueryTupleType>({ subtypes }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			} };
		}

		void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr) final {
			std::cout << "visitVariantTypeConstructorExpr\n";
			std::vector<tsh::SymbolType<>> subtypes;
			for (auto& sub_type: expr.subtypes) {
				// should we here short-path or not?
				auto sub_type_result
					= ctx.query<QueryEvaluateHoutExpressionCT>({ sub_type.operator->() });
				if (sub_type_result.hasError()) {
					result = query::QError(errors::Failed(sub_type_result.error()));
					return;
				}

				variant_match(sub_type_result.value()) {
					variant_case(tsh::SymbolType<>, type) { subtypes.emplace_back(type); }
					variant_default { CORE_PANIC("Type evaluation returned not a type\n"); }
				}
			}

			result = CTV{ tsh::SymbolType<>{
				ctx.query<tsh::QueryVariantType>({ subtypes }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			} };
		}

		void visitAccessExpr(const code::AccessExpr&) final {
			std::cout << "visitAccessExpr\n";
			throw base::NotYetImplemented("Evaluation of access expressions is not implemented yet");
		}

		void visitSequenceExpr(const code::SequenceExpr& seq) final {
			std::cout << "visitSequenceExpr\n";
			result = ctx.query<compiler::helios::QueryEvaluateHoutExpressionCT>(
				{ seq.expressions.back().operator->() }
			);
		}
	};
}

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryEvaluateHoutExpressionCT, CompTimeEvalResult) {
		static bool isSimpleEnoughForTreeEval(const code::Expr& expr) {
			IsSimpleVisitor visitor;
			expr.acceptVisitor(visitor);
			return visitor.is_simple;
		}

		static CompTimeEvalResult evaluateWithTreeEval(query::Context& ctx, const code::Expr& expr) {
			std::cout << "Hello from evaluateWithTreeEval\n";

			TreeEvalVisitor visitor(ctx);
			expr.acceptVisitor(visitor);
			return std::move(visitor.result);
		}

		static CompTimeEvalResult evaluateFunctionWithVm(
			query::Context& ctx, const code::Expr& expr
		) {
			std::cout << "Hello from evaluateWithVm\n";
			const auto* call_expr = dynamic_cast<const code::CallExpr*>(&expr);
			// TODOP: For now VM is only used for function call evaluation.
			if (!call_expr) return query::QError(errors::Failed());

			const auto* callee_ident
				= dynamic_cast<const code::IdentifierExpr*>(call_expr->callee.operator->());
			if (!callee_ident) return query::QError(errors::Failed());

			const SymID function_sym_id = callee_ident->symbol;

			std::cout << "Query Code of fun\n";

			// https://github.com/ducktype-org/duckling/issues/826
			auto fun_hout_result = ctx.query<QueryCodeOFFun>(function_sym_id);
			// TODOP: Error checking here?

			auto mir_func_result = ctx.query<mir::LowerToMirFunction>({ fun_hout_result });
			if (mir_func_result->hasError()) return query::QError(mir_func_result->error());
			// TODOP: Error checking here?

			const mir::Function& mir_func = mir_func_result->value();
			// TODOP: This may be unsafe? But the function stays in the query's cache so maybe not.
			CRef<mir::Function> mir_func_cref{ &mir_func };

			auto lir_func_result = ctx.query<lir::LowerToLirFunction>({ mir_func_cref });
			// TODOP: Error checking here?
			std::cout << "LIR dump:\n";
			lir_func_result->debugPrint(ctx, std::cout);
			std::cout << "\n";

			// Get code of the called function.
			// TODOP: Maybe add create a backend_vm::Function and don't use Module everywhere?
			backend_vm::Module       m{ ctx, base::StrID("COMP_TIME"), { lir_func_result }, {} };
			vm::code::CodeCollection code = m.build();

			// TODOP: Problem. What with functions which invoke other functions? We should loop
			// recursively through the whole function to look for subfunctions?

			std::cout << "Got code collection\n";

			std::vector<CTV> ctv_arguments;
			for (const auto& arg_expr: call_expr->arguments) {
				auto arg_result
					= ctx.query<QueryEvaluateHoutExpressionCT>({ arg_expr.operator->() });
				if (arg_result.hasError()) return arg_result;
				ctv_arguments.push_back(arg_result.value());
			}

			auto vm_eval_result = CompileTimeEvaluator::get().executeInVm(
				// Is the compiler return type needed here?
				callee_ident->expression_type
					.getSymbolType(),  // TODOP: Thats wrong. How to get a return type of the
			                           // function from somewhere?
				code,
				lir_func_result->mangled_name.str(),
				ctv_arguments
			);

			if (vm_eval_result.has_value())
				return vm_eval_result.value();
			else
				return query::QError(vm_eval_result.error());
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			std::cout << "Hello from QueryEvaluateHoutExpressionCT\n";
			key.expr->debugPrint(std::cout);
			std::cout << '\n';
			// TODOP: This can be optimized. Always try to eval with TreeEval and only use VM
			// eval when failed.
			if (isSimpleEnoughForTreeEval(*key.expr)) return evaluateWithTreeEval(ctx, *key.expr);
			// return evaluateWithTreeEval(ctx, *key.expr);

			std::cout << "Expression complicated: Evaluate with VM\n";
			return evaluateFunctionWithVm(ctx, *key.expr);
		}

		// TODOP: No cache because im broken.
		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluateHoutExpressionCT);

	struct IMPLEMENT_QUERY(QueryEvaluateExpressionCT, CompTimeEvalResult) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			std::cout << "===========================================\n";
			std::cout << "Hello from QueryEvaluateExpressionCT\n";
			// TODOP: Simple query just to initialise the recursion.
			auto eval = ctx.query<QueryHoutOfExpr>({ key.element });
			if (eval.hasError()) return query::QError(errors::Failed());
			return ctx.query<QueryEvaluateHoutExpressionCT>({ eval.value().operator->() });
		}

		//
		// TODOP: No cache because im broken.
		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluateExpressionCT);
}
