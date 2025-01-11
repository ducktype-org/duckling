#include <query_framework/query_impl.hpp>
#include <pst_parser/pst_expr_visitor.hpp>

#include <base/optional.hpp>
#include <base/box.hpp>

#include "../../scopes/scopes.hpp"
#include "../visitors.hpp"
#include "query_hout_of_expr.hpp"
#include "expr.hpp"

namespace compiler::helios::code {
	namespace {

		/**
		 * This is an effective implementation of QueryHoutOfExpr.
		 * QueryHoutOfExpr is mostly a wrapper for future cache.
		 * @note This is a private function of this file.
		 */
		ExprConstructionResult fromPST(query::Context& ctx, MCRef<pst::ExprElement> element);

		void getVariantSubExprsInPlace(
			MCRef<pst::ExprElement> expr, std::vector<CRef<pst::ExprElement>>& sub_exprs_append
		) {
			if (auto bin_op = dynamic_cast<const pst::expr::BinaryOperator*>(&*expr)) {
				if (bin_op->getOperator().str() == "|") {
					getVariantSubExprsInPlace(bin_op->getLeftOperand(), sub_exprs_append);
					getVariantSubExprsInPlace(bin_op->getRightOperand(), sub_exprs_append);
				}
			} else {
				sub_exprs_append.emplace_back(expr.toOpt().value());
			}
		}

		/**
		 * @brief Extracts sub expressions from a variant operator.
		 * This flattens PST `a | b | c` expression (only if there are no parenthesis).
		 */
		std::vector<CRef<pst::ExprElement>> getVariantSubExprs(const pst::expr::BinaryOperator& expr
		) {
			CORE_ASSERT(expr.getOperator().str() == "|", "Not a variant operator");
			std::vector<CRef<pst::ExprElement>> sub_exprs;
			getVariantSubExprsInPlace(expr.getLeftOperand(), sub_exprs);
			getVariantSubExprsInPlace(expr.getRightOperand(), sub_exprs);
			return sub_exprs;
		}

		/**
		 * @brief Tries to extract a resulting symbol from hout expression.
		 * @note Logic like this might be useful one day for "go-to-definition" on expressions,
		 * but it might get removed from hout creation in the future.
		 */
		struct HoutResultingSymbolListVisitor final: public HoutExprVisitorPanicky {
			explicit HoutResultingSymbolListVisitor(query::Context& ctx, ScopeID scope):
				  ctx(ctx),
				  scope(scope) {}

			query::Context& ctx;
			ScopeID         scope;

			base::Optional<SymbolList> symbols;

			void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override {
				// note: it should be possible if given operator points to a
				// user defined operator.
				throw base::NotYetImplemented("Cannot evaluate symbol after binary operators");
			}

			void visitIdentifierExpr(const IdentifierExpr& val) override {
				symbols = SymbolList{ val.symbol };
			}

			void visitParenthesisExpr(const ParenthesisExpr& val) override {
				HoutResultingSymbolListVisitor vis(ctx, scope);
				val.inner->acceptVisitor(vis);
				symbols = vis.symbols;
			}

			void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {
				// note: it should be possible if given operator points to a
				// user defined operator.
				throw base::NotYetImplemented("Cannot evaluate symbol after unary operators");
			}

			void visitLinkedIdentifierExpr(const LinkedIdentifierExpr& val) override {
				symbols = val.symbols;
			}
		};

		struct PstExprToHoutExprVisitor final: public pst::PstExprVisitorPanicky {
			explicit PstExprToHoutExprVisitor(query::Context& ctx, ScopeID scope):
				  ctx(ctx),
				  scope(scope) {}

			query::Context& ctx;
			ScopeID         scope;

			base::Optional<base::Box<Expr>> node;

			void visitExprValue(const pst::expr::ExprValue& stmt) override {
				// @TODO: Change literal value from i64 to something more appropriate.
				node = makeBox<LiteralIntExpr>(ctx, scope, std::stoi(stmt.getValue().str()));
			}

			void visitBinaryOperator(const pst::expr::BinaryOperator& stmt) override {
				// handle variants:
				if (stmt.getOperator().str() == "|") {
					// @todo HOUT 2.0:
					// Here we assume that "|" always produces a variant (likely valid).
					// If it does not, and "|" will remain a binary operator,
					// we will have to do something with it.
					// (likely if-out if all sub expressions are meta or non-meta, throw otherwise,
					// (require parentheses))

					auto sub_exprs = getVariantSubExprs(stmt);
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
					node = makeBox<VariantTypeConstructorExpr>(ctx, scope, std::move(all_subtypes));
					return;
				}


				auto lhs_res = fromPST(ctx, stmt.getLeftOperand());
				auto rhs_res = fromPST(ctx, stmt.getRightOperand());

				if (lhs_res.hasError() or rhs_res.hasError()) return;  // failed

				auto lhs = std::move(lhs_res).value();
				auto rhs = std::move(rhs_res).value();

				// @todo here we should type check,
				// and make function call / builtin binary operator
				node = makeBox<BinaryOperatorExpr>(
					ctx, scope, stmt.getOperator(), std::move(lhs), std::move(rhs)
				);
			}

			void visitChainExpr(const pst::expr::ChainExpr& stmt) override {
				// @TODO: Add a compiler log or some kind of information if lookup fails.

				auto literal_expr = fromPST(ctx, stmt.getLiteral());
				if (!literal_expr) {
					// Report an error?
					return;
				}

				HoutResultingSymbolListVisitor resulting_symbol_vis(ctx, scope);
				literal_expr.value()->acceptVisitor(resulting_symbol_vis);

				CORE_ASSERT(resulting_symbol_vis.symbols, "Failed to get symbols");

				SymbolList looked_up_symbol = std::move(resulting_symbol_vis.symbols.value());

				for (auto&& el: stmt.getChain()) {
					auto pst_access = dynamic_cast<pst::expr::Access*>(&*el);
					CORE_ASSERT(pst_access, "Not handling non-AccessExprs yet");
					CORE_ASSERT(
						pst_access->getType() == ".", "Not handling .? access operator yet"
					);

					std::cout << "Lookup in: " << name(looked_up_symbol.back()).strView() << " "
							  << pst_access->getName().value.strView() << "\n";
					auto new_symbols = *ctx.query<QueryLookupInSymbol>(
						{ looked_up_symbol.back(), pst_access->getName().value, true }
					);

					auto new_symbols_single = new_symbols.getAsSingle();
					CORE_ASSERT(
						new_symbols_single.hasValue(),
						"Access failed because couldn\'t getAsSingle()"
					);

					looked_up_symbol.insert(
						looked_up_symbol.end(),
						new_symbols_single.value().begin(),
						new_symbols_single.value().end()
					);
				}
				if_opt_some(dealiasSymbolList(ctx, looked_up_symbol).optValueMove(), dealiased) {
					node = makeBox<LinkedIdentifierExpr>(ctx, scope, std::move(dealiased));
				}
			}

			void visitRoundExpr(const pst::expr::RoundExpr& stmt) override {
				PstExprToHoutExprVisitor vis(ctx, scope);
				stmt.getInner()->acceptExprVisitor(vis);
				if (vis.node) node = makeBox<ParenthesisExpr>(ctx, scope, std::move(*vis.node));
			}

			void visitIdentifierLiteral(const pst::expr::IdentifierLiteral& stmt) override {
				auto&& sym_list
					= *ctx.query<QueryLookupInScopeAndParents>({ scope, stmt.getName().value, true }
				    );

				auto res = sym_list.getAsSingle();
				if (res.hasError()) {
					// Report an error
					return;
				}

				if_opt_some(dealiasSymbolList(ctx, res.value()).optValueMove(), dealiased) {
					node = makeBox<IdentifierExpr>(ctx, scope, dealiased.back());
				}
			}

			void visitKeywordLiteral(const pst::expr::KeywordLiteral& stmt) override {
				switch (stmt.getKeyword()) {
				// true, false:
				case pst::Keyword::True:
					node = makeBox<LiteralBoolExpr>(ctx, scope, true);
					break;
				case pst::Keyword::False:
					node = makeBox<LiteralBoolExpr>(ctx, scope, false);
					break;


				// types:
				case pst::Keyword::Bool:
					node = makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryBoolType>({}));
					break;

				case pst::Keyword::Char:
					node = makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryCharType>({}));
					break;

					// @todo: add meta keyword and type

				case pst::Keyword::i128:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 128, true })
					);
					break;
				case pst::Keyword::i64:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 64, true })
					);
					break;
				case pst::Keyword::i32:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 32, true })
					);
					break;
				case pst::Keyword::i16:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 16, true })
					);
					break;
				case pst::Keyword::i8:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 8, true })
					);
					break;

				case pst::Keyword::u128:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 128, false })
					);
					break;
				case pst::Keyword::u64:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 64, false })
					);
					break;
				case pst::Keyword::u32:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 32, false })
					);
					break;
				case pst::Keyword::u16:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 16, false })
					);
					break;
				case pst::Keyword::u8:
					node = makeBox<LiteralTypeExpr>(
						ctx, scope, ctx.query<tsh::QueryIntegralType>({ 8, false })
					);
					break;

				case pst::Keyword::f80:
					node = makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryFloatType>(80));
					break;
				case pst::Keyword::f128:
					node
						= makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryFloatType>(128));
					break;
				case pst::Keyword::f64:
					node = makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryFloatType>(64));
					break;
				case pst::Keyword::f32:
					node = makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryFloatType>(32));
					break;
				case pst::Keyword::f16:
					node = makeBox<LiteralTypeExpr>(ctx, scope, ctx.query<tsh::QueryFloatType>(16));
					break;


				default:
					CORE_PANIC(
						"Keyword not yet handled (or bad keyword) by PstExprToHoutExprVisitor"
					);
				}
			}

			void visitComma(const pst::expr::Comma& stmt) override {
				std::vector<Box<Expr>> expressions;
				for (auto& ex: stmt.getExpressions()) {
					auto res = fromPST(ctx, ex.ref());
					if (res.hasError()) {
						// Error has occurred.
						return;
					}
					expressions.emplace_back(std::move(res).value());
				}

				node = makeBox<TupleTypeConstructorExpr>(ctx, scope, std::move(expressions));
			}

			void visitSuffixOperator(const pst::expr::SuffixOperator& stmt) override {
				// @NOTE: This is a mockup
				PstExprToHoutExprVisitor vis(ctx, scope);
				stmt.getExpr()->acceptExprVisitor(vis);
				if_opt_some(vis.node, expr) {
					node = makeBox<UnaryOperatorExpr>(
						scope, stmt.getOperator(), false, std::move(expr)
					);
				}
			}

			void visitPrefixOperator(const pst::expr::PrefixOperator& stmt) override {
				// @NOTE: This is a mockup
				PstExprToHoutExprVisitor vis(ctx, scope);
				stmt.getExpr()->acceptExprVisitor(vis);
				if_opt_some(vis.node, expr) {
					node = makeBox<UnaryOperatorExpr>(
						scope, stmt.getOperator(), true, std::move(expr)
					);
				}
			}
		};

		ExprConstructionResult fromPST(query::Context& ctx, MCRef<pst::ExprElement> element) {
			auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ element });

			// std::cerr << "\nExpr: \n";
			// root->debugPrint(std::cerr);
			// std::cerr << '\n'

			PstExprToHoutExprVisitor visitor(ctx, scope);
			element->acceptExprVisitor(visitor);

			if_opt_some(visitor.node, expr) return std::move(expr);
			return errors::HError(errors::Failed());
		}

	}
}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// Note: we might actually accept nulls in such queries, and just return failed
			// Something to think about as part of #412
			CORE_ASSERT(key.element.toOpt().has_value(), "Nullptr provided to QueryHoutOfExpr");

			// @TODO static assert this is top-expr
			return code::fromPST(ctx, key.element);
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)
}
