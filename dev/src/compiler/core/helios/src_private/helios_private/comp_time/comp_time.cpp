#include "comp_time.hpp"

#include "helios/helios_errors.hpp"

#include <backends/dvm/backend.hpp>
#include <helios/ctv/ctv.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios_private/comp_time/vm_evaluator.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <pst_parser/elements/includes/basic.hpp>
#include <typesystem/higher/queries/types.hpp>

#include "base/exceptions.hpp"
#include "base/variant.hpp"

#include "query_framework/query_result.hpp"
#include <query_framework/context.hpp>
#include <query_framework/query_cache_macros.hpp>
#include <query_framework/query_impl.hpp>

#include <vm/bytecode/bytecode.hpp>

#include <cmath>
#include <iostream>

using namespace compiler::helios;

struct IMPLEMENT_QUERY(QueryCompTime, CompTimeEvalResult) {
	/**
	 * @brief Error indicating that an expression was to complex for simple tree evaluation.
	 */
	struct CouldNotShortPath {};

	using TreeEvalResult = query::QResult<CTV, CouldNotShortPath, errors::Failed>;

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
			result = query::QError(CouldNotShortPath{});
		}

		void visitAccessExpr(const code::AccessExpr&) final {
			std::cout << "visitAccessExpr\n";
			result = query::QError(CouldNotShortPath{});
		}

		void visitIdentifierExpr(const code::IdentifierExpr& expr) final {
			std::cout << "visitIdentifierExpr\n";
			// Type Evaluation.
			if (expr.expression_type.getType().getKind() == tsh::Kind::Meta) {
				auto type = ctx.query<QueryTypeFromDefinition>({ expr.symbol });
				result    = type->hasValue() ? CompTimeEvalResult{ CTV{ type->value() } }
				                             : query::QError(errors::Failed());
			} else {
				// Constant Evaluation.
				auto const_val_result = ctx.query<QueryConstValueOf>({ expr.symbol });
				if (const_val_result.hasValue())
					result = CompileTimeValue{ const_val_result.value() };
				else
					result = query::QError(const_val_result.error());
			}
		}

		void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) final {
			std::cout << "visitBinaryOperatorExpr\n";
			auto lhs_result = evalHoutExpr(ctx, expr.lhs.ref());
			if (lhs_result.hasError()) {
				result = query::QError(errors::Failed(lhs_result.error()));
				return;
			}

			auto rhs_result = evalHoutExpr(ctx, expr.rhs.ref());
			if (rhs_result.hasError()) {
				result = query::QError(errors::Failed(rhs_result.error()));
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
									"Evaluation of different than '+-*/%**' binary operators"
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
			auto expr_result = evalHoutExpr(ctx, expr.expr.ref());
			if (expr_result.hasError()) {
				result = query::QError(errors::Failed(expr_result.error()));
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
			auto cond_result = evalHoutExpr(ctx, expr.condition.ref());
			if (cond_result.hasError()) {
				result = query::QError(errors::Failed(cond_result.error()));
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
				result = evalHoutExpr(ctx, expr.if_true.ref());
			else
				result = evalHoutExpr(ctx, expr.if_false.ref());
		}

		void visitParenthesisExpr(const code::ParenthesisExpr& expr) final {
			std::cout << "visitParenthesisExpr\n";
			result = evalHoutExpr(ctx, expr.inner.ref());
		}

		void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) final {
			std::cout << "visitTupleTypeConstructorExpr\n";
			std::vector<tsh::SymbolType<>> subtypes;

			for (auto& sub_type: expr.elements) {
				auto sub_type_result = evalHoutExpr(ctx, sub_type.ref());
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
				auto sub_type_result = evalHoutExpr(ctx, sub_type.ref());
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

		void visitSequenceExpr(const code::SequenceExpr& seq) final {
			std::cout << "visitSequenceExpr\n";
			result = evalHoutExpr(ctx, seq.expressions.back().ref());
		}
	};

	/**
	 * @brief Evaluates a HOUT expression using VM Eval.
	 * @return The calculated result represented by CTV or a Failed error.
	 */
	static CompTimeEvalResult evaluateFunctionWithVm(query::Context& ctx, CRef<code::Expr> expr) {
		// TODOP: Make this more generic and work for other things than calls only.
		// TODOP: For now VM is only used for function call evaluation.
		// TODOP: https://github.com/ducktype-org/duckling/issues/826
		// TODOP: Problem. What with functions which invoke other functions? We should loop
		// recursively through the whole function to look for subfunctions?
		using namespace compiler;

		std::cout << "Hello from evaluateFunctionWithVm\n";
		const auto* call_expr = dynamic_cast<const code::CallExpr*>(expr.get());
		if (!call_expr) return query::QError(errors::Failed());

		const auto* callee_ident
			= dynamic_cast<const code::IdentifierExpr*>(call_expr->callee.operator->());
		if (!callee_ident) return query::QError(errors::Failed());

		const SymID function_sym_id = callee_ident->symbol;

		std::cout << "Query Code of fun\n";

		// Get code of the called function.
		auto fun_hout_result = ctx.query<QueryCodeOFFun>(function_sym_id);

		auto mir_func_result = ctx.query<mir::LowerToMirFunction>({ fun_hout_result });
		if (mir_func_result->hasError()) return query::QError(mir_func_result->error());

		CRef<mir::Function> mir_func        = &mir_func_result->value();
		auto                lir_func_result = ctx.query<lir::LowerToLirFunction>({ mir_func });

		// TODOP: Maybe add create a backend_vm::Function and don't use Module everywhere?
		backend_vm::Module       m{ ctx, base::StrID("COMP_TIME"), { lir_func_result }, {} };
		vm::code::CodeCollection code = m.build();

		// TODOP: Remove that.
		std::cout << "Got code collection\n";

		std::vector<CTV> ctv_arguments;
		for (const auto& arg_expr: call_expr->arguments) {
			auto arg_result = evalHoutExpr(ctx, arg_expr.ref());
			if (arg_result.hasError()) return arg_result;
			ctv_arguments.push_back(arg_result.value());
		}

		auto vm_eval_result = CompileTimeEvaluator::get().executeInVm(
			// Is the compiler return type needed here?
			callee_ident->expression_type.getSymbolType(
			),  // TODOP: Thats wrong. How to get a return
		        // type of the function from somewhere?
			code,
			lir_func_result->mangled_name.str(),
			ctv_arguments
		);

		if (vm_eval_result.has_value())
			return vm_eval_result.value();
		else
			return query::QError(vm_eval_result.error());
	}

	/**
	 * @brief Evaluates a HOUT expression using TreeEval.
	 * @return The calculated result represented by CTV, a CouldNotShortPath error if the expresion
	 * was to complicated for tree eval or a Failed error.
	 */
	static auto evaluateWithTreeEval(query::Context& ctx, CRef<code::Expr> expr) -> TreeEvalResult {
		std::cout << "Hello from evaluateWithTreeEval\n";
		TreeEvalVisitor visitor(ctx);
		expr->acceptVisitor(visitor);
		return visitor.result;
	}

	/**
	 * @brief Evaluates a HOUT expression using TreeEval or VMEval if the expression is to complex
	 * for tree eval.
	 * @return The calculated result represented by CTV or a Failed error.
	 */
	static auto evalHoutExpr(query::Context& ctx, CRef<code::Expr> expr) -> PResult {
		// Try evaluating with TreeEval(Short Path).
		TreeEvalResult tree_eval_result = evaluateWithTreeEval(ctx, expr);

		if (tree_eval_result.hasError()) {
			variant_match(tree_eval_result.error()) {
				variant_case(errors::Failed, failed) {
					return query::QError(errors::Failed(failed));
				}
				variant_case(CouldNotShortPath, _) {
					// If TreeEval failed, try to evaluate with VM.
					// TODOP: Generic evaluateWithVM not just function calls.
					return evaluateFunctionWithVm(ctx, expr);
				}
			}
		}
		return tree_eval_result.value();
	}

	static auto provide(query::Context& ctx, QKey key) -> PResult {
		std::cout << "===========================================\n";
		std::cout << "Hello from QueryCompTime provide\n";

		auto expr = ctx.query<QueryHoutOfExpr>({ key.element });
		if (expr.hasError()) return query::QError(errors::Failed(expr.error()));

		return evalHoutExpr(ctx, expr.value().ref());
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(QueryCompTime);
