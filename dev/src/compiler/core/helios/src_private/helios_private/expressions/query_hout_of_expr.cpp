#include "query_hout_of_expr.hpp"

#include "coercions.hpp"
#include "errors.hpp"
#include "numeric_literals.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios_private/expressions/builtin_operations.hpp>
#include <helios_private/expressions/chain_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <typesystem/higher/queries.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::helios::code {
	namespace {

		/**
		 * This is an effective implementation of QueryHoutOfExpr.
		 * QueryHoutOfExpr is mostly a wrapper for future cache.
		 * @note This is a private function of this file.
		 */
		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		);

		void getVariantSubExprsInPlace(
			query::Context&                                   ctx,
			pst::AccessLocked<pst::ExprElement>               expr,
			std::vector<pst::AccessLocked<pst::ExprElement>>& sub_exprs_append
		) {
			if (auto bin_op_opt = expr.unlock(ctx).dynamicCast<pst::expr::BinaryOperator>()) {
				auto bin_op = bin_op_opt.value();
				if (bin_op->getOperator().str() == "|") {
					getVariantSubExprsInPlace(ctx, bin_op->getLeftOperand(), sub_exprs_append);
					getVariantSubExprsInPlace(ctx, bin_op->getRightOperand(), sub_exprs_append);
				}
			} else {
				sub_exprs_append.emplace_back(expr);
			}
		}

		/**
		 * @brief Extracts sub expressions from a variant operator.
		 * This flattens PST `a | b | c` expression (only if there are no parenthesis).
		 */
		std::vector<pst::AccessLocked<pst::ExprElement>> getVariantSubExprs(
			query::Context& ctx, pst::AccessLocked<pst::expr::BinaryOperator> expr
		) {
			CORE_ASSERT(expr.unlock(ctx)->getOperator().str() == "|", "Not a variant operator");
			std::vector<pst::AccessLocked<pst::ExprElement>> sub_exprs;
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getLeftOperand(), sub_exprs);
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getRightOperand(), sub_exprs);
			return sub_exprs;
		}

		/**
		 * Visitor that implements logic of creation of HOUT expressions from PST expressions.
		 */
		struct PstExprToHoutExprVisitor final: public pst::expr::PstExprVisitorPanicky {
			explicit PstExprToHoutExprVisitor(query::Context& ctx): ctx(ctx) {}

			query::Context& ctx;

			/**
			 * The "output" of the visitor.
			 */
			base::Optional<base::Box<Expr>> node;

			void visitUnitExpr(pst::Access<pst::expr::UnitExpr>) override {
				node = makeBox<LiteralUnitExpr>(ctx);
			}

			void visitExprValue(pst::Access<pst::expr::ExprValue> stmt) override {
				auto parsed_numeric_value = fromExprValue(ctx, stmt);

				if (parsed_numeric_value.has_value()) {
					node = makeBox<LiteralNumericExpr>(ctx, parsed_numeric_value.value());
				} else {
					// Error was logged in fromExprValue.
					return;
				}
			}

			void visitExprStrValue(pst::Access<pst::expr::ExprStrValue> stmt) override {
				node = makeBox<LiteralStringExpr>(ctx, stmt->getValue());
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> binaryBuiltin(
				lexer::Operator op, Box<Expr> lhs, Box<Expr> rhs
			) {
				auto result = findBinaryBuiltin(ctx, op, lhs.ref(), rhs.ref());
				if (result) {
					auto [operation, lhs_coercion, rhs_coercion] = std::move(result).value();

					auto coerced_lhs = lhs_coercion.coerce(ctx, std::move(lhs));
					auto coerced_rhs = rhs_coercion.coerce(ctx, std::move(rhs));

					return makeBox<BinaryOperatorExpr>(
						ctx, operation, std::move(coerced_lhs), std::move(coerced_rhs)
					);
				}
				return {};
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> unaryBuiltin(lexer::Operator op, Box<Expr> expr) {
				auto operation = findUnaryBuiltin(op, expr.ref());

				if (operation)
					return makeBox<UnaryOperatorExpr>(operation.value(), std::move(expr));
				else
					return {};
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				// handle variants:
				if (stmt->getOperator().str() == "|") {
					// @todo HOUT 2.0:
					// Here we assume that "|" always produces a variant (likely valid).
					// If it does not, and "|" will remain a binary operator,
					// we will have to do something with it.
					// (likely if-out if all sub expressions are meta or non-meta, throw otherwise,
					// (require parentheses))

					auto sub_exprs = getVariantSubExprs(ctx, stmt);
					// @todo HOUT 2.0:
					// validate that all sub types are meta

					std::vector<Box<Expr>> all_subtypes;

					for (auto sub_expr: sub_exprs) {
						auto sub_expr_hout = fromPST(ctx, sub_expr);
						if (sub_expr_hout.hasError()) {
							// Error has occurred.
							return;
						}
						all_subtypes.emplace_back(std::move(sub_expr_hout).value());
					}
					node = makeBox<VariantTypeConstructorExpr>(ctx, std::move(all_subtypes));
					return;
				}

				auto lhs_res = fromPST(ctx, stmt->getLeftOperand());
				auto rhs_res = fromPST(ctx, stmt->getRightOperand());

				// @todo: make failure more explicit...
				if (lhs_res.hasError() or rhs_res.hasError()) return;  // failed

				auto lhs = std::move(lhs_res).value();
				auto rhs = std::move(rhs_res).value();

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:
				auto lhs_type = lhs->expression_type.getType();
				auto rhs_type = rhs->expression_type.getType();

				auto builtin = binaryBuiltin(stmt->getOperator(), std::move(lhs), std::move(rhs));
				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(makeBox<code::UndefinedBinaryOperator>(
						stmt->getSourcePosition(),
						stmt->getOperator().str(),
						lhs_type.toString(),
						rhs_type.toString()
					));
					// failed
				}
			}

			void visitChainExpr(pst::Access<pst::expr::ChainExpr> chain_expr) override {
				auto result = fromChainExpr(ctx, chain_expr);
				if (result.hasError()) {
					// Error has occurred.
					return;
				}
				node = std::move(result.value());
			}

			void visitRoundExpr(pst::Access<pst::expr::RoundExpr> stmt) override {
				PstExprToHoutExprVisitor vis(ctx);
				stmt->getInner().unlock(ctx)->acceptExprVisitor(vis);
				if (vis.node) node = makeBox<ParenthesisExpr>(ctx, std::move(*vis.node));
			}

			void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> stmt) override {
				// note: this is a mock, it should be unified with ChainExpr
				auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

				const auto& sym_list = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
					stmt->getName().position, ctx, stmt->getName().value
				);
				if (!sym_list) {
					// failed
					return;
				}

				node = makeBox<IdentifierExpr>(ctx, sym_list.value().back());
			}

			void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> stmt) override {
				using enum tsh::IntegralAbstractType::Signedness;
				switch (stmt->getKeyword()) {
				// true, false:
				case pst::Keyword::True:
					node = makeBox<LiteralBoolExpr>(ctx, true);
					break;
				case pst::Keyword::False:
					node = makeBox<LiteralBoolExpr>(ctx, false);
					break;


				// types:
				case pst::Keyword::Bool:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryBoolType>({}));
					break;

				case pst::Keyword::Char:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryCharType>({}));
					break;

				case pst::Keyword::Str:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryStringType>({}));
					break;

				case pst::Keyword::Type:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryMetaType>({}));
					break;

				case pst::Keyword::i128:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 128, Signed })
					);
					break;
				case pst::Keyword::i64:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 64, Signed })
					);
					break;
				case pst::Keyword::i32:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 32, Signed })
					);
					break;
				case pst::Keyword::i16:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 16, Signed })
					);
					break;
				case pst::Keyword::i8:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 8, Signed })
					);
					break;

				case pst::Keyword::u128:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 128, Unsigned })
					);
					break;
				case pst::Keyword::u64:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 64, Unsigned })
					);
					break;
				case pst::Keyword::u32:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 32, Unsigned })
					);
					break;
				case pst::Keyword::u16:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 16, Unsigned })
					);
					break;
				case pst::Keyword::u8:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 8, Unsigned })
					);
					break;

				case pst::Keyword::f80:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>({ 80 }));
					break;
				case pst::Keyword::f128:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>({ 128 }));
					break;
				case pst::Keyword::f64:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>({ 64 }));
					break;
				case pst::Keyword::f32:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>({ 32 }));
					break;
				case pst::Keyword::f16:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>({ 16 }));
					break;


				default:
					CORE_PANIC(
						"Keyword not yet handled (or bad keyword) by PstExprToHoutExprVisitor"
					);
				}
			}

			void visitComma(pst::Access<pst::expr::Comma> stmt) override {
				std::vector<Box<Expr>> expressions;
				for (auto ex: stmt->getExpressions()) {
					auto res = fromPST(ctx, ex);
					if (res.hasError()) {
						// Error has occurred.
						return;
					}
					expressions.emplace_back(std::move(res).value());
				}

				node = makeBox<TupleExpr>(ctx, std::move(expressions));
			}

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator>) override {
				// note: here we will have to compile things like `a++`, `a--`, `T?`.
				throw base::NotYetImplemented(
					"Suffix operators are not yet implemented in HOUT, since there are any for now"
				);
			}

			void visitPrefixOperator(pst::Access<pst::expr::PrefixOperator> stmt) override {
				// @NOTE: This is a mockup
				auto inner = fromPST(ctx, stmt->getExpr());
				if (inner.hasError()) return;  // failed

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				auto expr_type = inner.value()->expression_type.getType();
				auto builtin   = unaryBuiltin(stmt->getOperator(), std::move(inner.value()));

				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(makeBox<UndefinedUnaryOperator>(
						stmt->getSourcePosition(), stmt->getOperator().str(), expr_type.toString()
					));
					// failed
				}
			}

			void visitTernary(pst::Access<pst::expr::Ternary> stmt) override {
				auto condition_res = fromPST(ctx, stmt->getCondition());
				auto if_true_res   = fromPST(ctx, stmt->getIfTrue());
				auto if_false_res  = fromPST(ctx, stmt->getIfFalse());

				if (condition_res.hasError() or if_true_res.hasError() or if_false_res.hasError())
					return;

				auto condition = std::move(condition_res).value();
				auto if_true   = std::move(if_true_res).value();
				auto if_false  = std::move(if_false_res).value();

				node = makeBox<TernaryOperatorExpr>(
					ctx, std::move(condition), std::move(if_true), std::move(if_false)
				);
			}

			void visitComparisonChain(pst::Access<pst::expr::ComparisonChain> stmt) override {
				using namespace ::std::views;

				const auto& pst_operators  = stmt->getOperators();
				size_t      operator_count = std::ranges::size(pst_operators);
				size_t      expr_count     = operator_count + 1;

				std::vector<Box<Expr>> result_exprs;
				result_exprs.reserve(expr_count);
				for (size_t i = 0; i < expr_count; ++i) {
					auto result = fromPST(ctx, stmt->getSubExpr(i));
					if (result.hasError())
						return;
					else
						result_exprs.push_back(std::move(result.value()));
				}

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				std::vector<BuiltinBinary> operators;
				operators.reserve(operator_count);
				for (size_t i = 0; i < operator_count; ++i) {
					auto lhs_type = result_exprs.at(i)->expression_type.getType();
					auto rhs_type = result_exprs.at(i + 1)->expression_type.getType();

					auto result = findBinaryBuiltin(
						ctx,
						pst_operators.at(i),
						result_exprs.at(i).ref(),
						result_exprs.at(i + 1).ref()
					);

					if (result) {
						auto [op, lhs_coercion, rhs_coercion] = std::move(result).value();
						result_exprs[i] = lhs_coercion.coerce(ctx, std::move(result_exprs[i]));
						result_exprs[i + 1]
							= rhs_coercion.coerce(ctx, std::move(result_exprs[i + 1]));
						operators.push_back(op);
					} else {
						ctx.log(makeBox<code::UndefinedBinaryOperator>(
							stmt->getSourcePosition(),
							pst_operators.at(i).str(),
							lhs_type.toString(),
							rhs_type.toString()
						));

						return;
					}
				}

				node = makeBox<ChainComparisonExpr>(
					ctx, std::move(result_exprs), std::move(operators)
				);
			}
		};

		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		) {
			PstExprToHoutExprVisitor visitor(ctx);
			element.unlock(ctx)->acceptExprVisitor(visitor);

			if_opt_some(visitor.node, expr) return std::move(expr);
			return query::QError(errors::Failed());
		}
	}
}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// Note: we might actually accept nulls in such queries, and just return failed
			// Something to think about as part of #412
			CORE_ASSERT(
				key.element.unlockOpt(ctx).has_value(), "Nullptr provided to QueryHoutOfExpr"
			);

			// @TODO static assert this is top-expr
			return code::fromPST(ctx, key.element);
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)

	ExprConstructionResult getHoutOfExprWithExpectedType(
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		const tsh::SymbolType<>                          expected_type
	) {
		auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr.element });
		if (expr_hout_qresult.hasError()) return query::QError(expr_hout_qresult.error());
		auto expr_hout = std::move(expr_hout_qresult).value();

		const auto coercion_qresult
			= canCoerce(ctx, expr_hout->expression_type.getSymbolType(), expected_type);
		if (coercion_qresult.hasError()) {
			ctx.log(makeBox<CannotCoerceError>(
				pst_expr.element.unlock(ctx)->getSourcePosition(),
				expr_hout->expression_type.getSymbolType(),
				expected_type
			));
			return query::QError(errors::Failed());
		}
		return coercion_qresult.value().coerce(ctx, std::move(expr_hout));
	}
}
