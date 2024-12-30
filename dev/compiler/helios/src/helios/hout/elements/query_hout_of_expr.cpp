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
		 */
		struct HoutResultingSymbolListVisitor: public HoutExprVisitor {
			explicit HoutResultingSymbolListVisitor(query::Context& ctx, ScopeID scope):
				  ctx(ctx),
				  scope(scope) {}

			query::Context& ctx;
			ScopeID         scope;

			base::Optional<SymbolList> symbols;

			void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override {
				// ctx.query<tsh::internal::QueryInterfaceOfClass>()
				throw base::NotYetImplemented("Cannot evaluate symbol after binary operators");
			}

			void visitIdentifierExpr(const IdentifierExpr& val) override {
				symbols = SymbolList{ val.symbol };
			}

			void visitKeywordExpr(const KeywordExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol from Keywords");
			}

			void visitLiteralIntExpr(const LiteralIntExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol from literal values");
			}

			void visitParenthesisExpr(const ParenthesisExpr& val) override {
				HoutResultingSymbolListVisitor vis(ctx, scope);
				val.inner->acceptVisitor(vis);
				symbols = vis.symbols;
			}

			void visitTupleTypeConstructorExpr(const TupleTypeConstructorExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol from tuple");
			}

			void visitVariantTypeConstructorExpr(const VariantTypeConstructorExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol from tuple");
			}

			void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol after unary operators");
			}

			void visitLinkedIdentifierExpr(const LinkedIdentifierExpr& val) override {
				symbols = val.symbols;
			}
		};

		struct PstExprToHoutExprVisitor: public pst::PstExprVisitorPanicky {
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
				node = makeBox<KeywordExpr>(ctx, scope, stmt.getKeyword());
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
