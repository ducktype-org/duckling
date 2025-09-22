#include "comp_time.hpp"

#include <backends/dvm/backend.hpp>
#include <helios/ctv/ctv.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios_private/comp_time/vm_evaluator.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_querries.hpp>
#include <pst_parser/elements/includes/basic.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_cache_macros.hpp>
#include <query_framework/query_impl.hpp>

#include <vm/bytecode/bytecode.hpp>

#include <cmath>
#include <ranges>
#include <type_traits>

namespace compiler::helios {
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

			void visitLiteralIntExpr(const code::LiteralIntExpr& expr) final {
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
					              ? CompTimeEvalResult{ CompileTimeValue{ type->value() } }
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

				variant_match(lhs_ctv.getStorage()) {
					variant_case(i64, lhs_value) {
						variant_match(rhs_ctv.getStorage()) {
							variant_case(i64, rhs_value) {
								switch (expr.operation) {
								case code::BuiltinBinary::IntegerAdd:
									result = CompileTimeValue{ lhs_value + rhs_value };
									break;
								case code::BuiltinBinary::IntegerSub:
									result = CompileTimeValue{ lhs_value - rhs_value };
									break;
								case code::BuiltinBinary::IntegerMul:
									result = CompileTimeValue{ lhs_value * rhs_value };
									break;
								case code::BuiltinBinary::IntegerDiv:
									result = CompileTimeValue{ lhs_value / rhs_value };
									break;
								case code::BuiltinBinary::IntegerMod:
									result = CompileTimeValue{ lhs_value % rhs_value };
									break;
								case code::BuiltinBinary::IntegerPow:
									result = CompileTimeValue{
										static_cast<i64>(std::pow(lhs_value, rhs_value))
									};
									break;
								case code::BuiltinBinary::BooleanAnd:
									result = CompileTimeValue{ lhs_value and rhs_value };
									break;
								case code::BuiltinBinary::BooleanOr:
									result = CompileTimeValue{ lhs_value or rhs_value };
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
						variant_match(rhs_ctv.getStorage()) {
							variant_case(bool, rhs_value) {
								switch (expr.operation) {
								case code::BuiltinBinary::BooleanAnd:
									result = CompileTimeValue{ lhs_value and rhs_value };
									break;
								case code::BuiltinBinary::BooleanOr:
									result = CompileTimeValue{ lhs_value or rhs_value };
									break;
								default:
									result = query::QError(errors::Failed());
									throw base::NotYetImplemented(
										"Evaluation of different than '&&, ||' binary operators "
										"for booleans is not implemented yet"
									);
								}
							}
						}
					}
					variant_default { result = query::QError(compiler::helios::errors::Failed()); }
				}
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) final {
				auto expr_result = evalHoutExpr(ctx, expr.expr.ref());
				if (expr_result.hasError()) {
					result = query::QError(errors::Failed(expr_result.error()));
					return;
				}

				const auto& ctv = expr_result.value();

				variant_match(ctv.getStorage()) {
					variant_case(i64, val) {
						switch (expr.operation) {
						case code::BuiltinUnary::IntegerNegation:
							result = CompileTimeValue{ -val };
							break;
						default:
							result = query::QError(errors::Failed());
							break;
						}
					}
					variant_case(bool, val) {
						switch (expr.operation) {
						case code::BuiltinUnary::BooleanNot:
							result = CompileTimeValue{ not val };
							break;
						default:
							result = query::QError(errors::Failed());
							break;
						}
					}
					variant_case(tsh::SymbolType<>, type_val) {
						switch (expr.operation) {
						case code::BuiltinUnary::Ref:
							result = CompileTimeValue{
								type_val.withReferenceKind(tsh::ReferenceKind::Ref)
							};
							break;
						case code::BuiltinUnary::Box:
							result = CompileTimeValue{
								type_val.withReferenceKind(tsh::ReferenceKind::Box)
							};
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
						throw base::NotYetImplemented(
							"Evaluation of unary operators for other types."
						);
					}
				}
			}

			void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) final {
				auto cond_result = evalHoutExpr(ctx, expr.condition.ref());
				if (cond_result.hasError()) {
					result = query::QError(errors::Failed(cond_result.error()));
					return;
				}

				bool        condition_is_true = false;
				const auto& cond_ctv          = cond_result.value();
				variant_match(cond_ctv.getStorage()) {
					variant_case(bool, val) { condition_is_true = val; }
					variant_case(i64, val) { condition_is_true = (val != 0); }
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
				auto compare = [](i64 first, i64 second, code::BuiltinBinary operation) {
					using enum code::BuiltinBinary;
					switch (operation) {
					case IntegerLt:
						return first < second;
					case IntegerGt:
						return first > second;
					case IntegerLteq:
						return first <= second;
					case IntegerGteq:
						return first >= second;
					case IntegerEq:
						return first == second;
					case IntegerNeq:
						return first != second;
					default:
						CORE_UNREACHABLE();
					}
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
					if (!compare(*prev_value.asI64(), *next_value.asI64(), comp)) {
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

			void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) final {
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
				std::vector<tsh::SymbolType<>> subtypes;
				for (auto& sub_type: expr.subtypes) {
					// should we here short-path or not?
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
					ctx.query<tsh::QueryVariantType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				} };
			}

			void visitSequenceExpr(const code::SequenceExpr& seq) final {
				result = evalHoutExpr(ctx, seq.expressions.back().ref());
			}
		};

		/**
		 * @brief Evaluates a HOUT call expression using VM Eval.
		 * @return The calculated result represented by CompileTimeValue or a Failed error.
		 */
		static CompTimeEvalResult evaluateFunctionWithVm(query::Context& ctx, CRef<code::Expr> expr) {
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
			auto fun_hout_result = ctx.query<QueryCodeOFFun>(function_sym_id);

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
					variant_case(errors::Failed, failed) { return query::QError(errors::Failed()); }
					variant_case(CouldNotShortPath, _) {
						// If TreeEval failed, try to evaluate with VM.
						return evaluateFunctionWithVm(ctx, expr);
					}
				}
			}
			return tree_eval_result.value();
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto expr = ctx.query<QueryHoutOfExpr>({ key.element });
			if (expr.hasError()) return query::QError(errors::Failed());
			return evalHoutExpr(ctx, expr.value().ref());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEvaluateExpression);
}
