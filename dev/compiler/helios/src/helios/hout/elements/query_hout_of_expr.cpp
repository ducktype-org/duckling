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

		base::Optional<std::vector<base::Box<Expr>>> getVariantExpressions(base::Ref<Expr> expr) {
			if (auto variant = dynamic_cast<VariantConstructorExpr*>(expr.get()); variant)
				return std::move(variant->subtypes);
			return {};
		}

		/**
		 * @note this flatten variants, because there are binary operators in PST
		 * The construction is kind of weird, since we
		 * make lhs, rhs, and just extract subtypes from them.
		 */
		base::Box<VariantConstructorExpr> constructVariantFrom(
			query::Context& ctx, ScopeID scope, base::Box<Expr> lhs, base::Box<Expr> rhs
		) {
			std::vector<base::Box<Expr>> all_subtypes;

			for (auto&& expr: std::array{ std::move(lhs), std::move(rhs) }) {
				auto subtypes = getVariantExpressions(expr.refMut());
				if (subtypes)
					for (auto&& subtype: *subtypes) all_subtypes.emplace_back(std::move(subtype));
				else
					all_subtypes.emplace_back(std::move(expr));
			}

			return makeBox<VariantConstructorExpr>(ctx, scope, std::move(all_subtypes));
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

			void visitLiteralValueExpr(const LiteralValueExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol from literal values");
			}

			void visitParenthesisExpr(const ParenthesisExpr& val) override {
				HoutResultingSymbolListVisitor vis(ctx, scope);
				val.inner->acceptVisitor(vis);
				symbols = vis.symbols;
			}

			void visitTupleConstructorExpr(const TupleConstructorExpr&) override {
				throw base::NotYetImplemented("Cannot evaluate symbol from tuple");
			}

			void visitVariantConstructorExpr(const VariantConstructorExpr&) override {
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
				node = makeBox<LiteralValueExpr>(ctx, scope, std::stoi(stmt.getValue().str()));
			}

			void visitBinaryOperator(const pst::expr::BinaryOperator& stmt) override {
				PstExprToHoutExprVisitor lhs(ctx, scope);
				PstExprToHoutExprVisitor rhs(ctx, scope);
				stmt.getLeftOperand()->acceptVisitor(lhs);
				stmt.getRightOperand()->acceptVisitor(rhs);

				if (lhs.node && rhs.node) {
					// clang format off
					// commented out, so we can remember about it in upcoming HOUT PRs:
					// HoutIsTypeExprVisitor lhs_vis_expr(ctx, scope);
					// HoutIsTypeExprVisitor rhs_vis_expr(ctx, scope);
					// lhs.node.value()->acceptVisitor(lhs_vis_expr);
					// rhs.node.value()->acceptVisitor(rhs_vis_expr);

					// if (lhs_vis_expr.is_type_expr != rhs_vis_expr.is_type_expr) {
					// 	// @TODO: Report an error
					// 	return;
					// }

					if (stmt.getOperator().str() == "|") {
						// @todo HOUT 2.0:
						// here we assume that "|" always produces a variant (likely valid)
						// put constructVariantFrom treats types incorrectly,
						// as its type is not "meta", but the variant itself
						node = constructVariantFrom(
							ctx, scope, std::move(*lhs.node), std::move(*rhs.node)
						);
					} else {
						node = makeBox<BinaryOperatorExpr>(
							ctx,
							scope,
							stmt.getOperator(),
							std::move(*lhs.node),
							std::move(*rhs.node)
						);
					}
					// clang format on
				}
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
				stmt.getInner()->acceptVisitor(vis);
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
				for (auto&& ex: stmt.getExpressions()) {
					PstExprToHoutExprVisitor vis(ctx, scope);
					ex->acceptVisitor(vis);
					if (!vis.node) {
						// Error has occurred.
						return;
					}
					if_opt_some(vis.node, b) { expressions.emplace_back(std::move(b)); }
				}

				node = makeBox<TupleConstructorExpr>(ctx, scope, std::move(expressions));
			}

			void visitSuffixOperator(const pst::expr::SuffixOperator& stmt) override {
				// @NOTE: This is a mockup
				PstExprToHoutExprVisitor vis(ctx, scope);
				stmt.getExpr()->acceptVisitor(vis);
				if_opt_some(vis.node, expr) {
					node = makeBox<UnaryOperatorExpr>(
						scope, stmt.getOperator(), false, std::move(expr)
					);
				}
			}

			void visitPrefixOperator(const pst::expr::PrefixOperator& stmt) override {
				// @NOTE: This is a mockup
				PstExprToHoutExprVisitor vis(ctx, scope);
				stmt.getExpr()->acceptVisitor(vis);
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
			element->acceptVisitor(visitor);

			if_opt_some(visitor.node, expr) return std::move(expr);
			return errors::HError(errors::Failed());
		}

	}
}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO static assert this is top-expr
			return code::fromPST(ctx, key.expr);
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)

	base::HashT KeyOf_QueryHoutOfExpr::customPerfectHash() const {
		auto hash_1 = this->expr->getID().asInt();

		return hash_1;
	}
}
