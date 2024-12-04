/**
 * @file pst_expr_to_hout_expr.cpp
 * @brief This file defines the conversion method Expr::fromPST
 * and the necessary auxiliary visitors.
 */

#include <base/optional.hpp>
#include <base/box.hpp>
#include <query_framework/query_impl.hpp>

#include "expr.hpp"
#include "../visitors.hpp"
#include "pst_parser/pst_expr_visitor.hpp"

namespace compiler::helios::code {
	namespace {
		base::Optional<std::vector<base::Box<Expr>>> getVariantExpressions(base::Ref<Expr> expr) {
			if (auto variant = dynamic_cast<VariantConstructorExpr*>(expr.get()); variant)
				return std::move(variant->subtypes);
			return {};
		}

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
	}

	struct HoutIsTypeExprVisitor: public HoutExprVisitor {
		// @TODO czy to nie powinno być roboione na poziomie HELIOS'a, żeby sprawdzać, czy
		// użytkownik nie próbuje użyć typu jako wartości lub odwrotnie? Czyli żeby rzucić błędem,
		// jeśli napisze: `let a: i32 + 13 = 20;`
		explicit HoutIsTypeExprVisitor(query::Context& ctx, ScopeID scope):
			  ctx(ctx),
			  scope(scope) {}

		bool            is_type_expr = false;
		query::Context& ctx;
		ScopeID         scope;

		void visitLiteralValueExpr(const LiteralValueExpr&) override { is_type_expr = false; }

		void visitIdentifierExpr(const IdentifierExpr& expr) override { testSymbol(expr.symbol); }

		void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override { is_type_expr = false; }

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override { is_type_expr = false; }

		void visitParenthesisExpr(const ParenthesisExpr& expr) override {
			HoutIsTypeExprVisitor vis(ctx, scope);
			expr.inner->acceptVisitor(vis);
			is_type_expr = vis.is_type_expr;
		}

		void visitKeywordExpr(const KeywordExpr& expr) override {
			using Keyword = lang_def::Keyword;
			switch (expr.keyword) {
			case Keyword::None:
			case Keyword::True:
			case Keyword::False:
				is_type_expr = false;
				break;
			case Keyword::i8:
			case Keyword::i16:
			case Keyword::i32:
			case Keyword::i64:
			case Keyword::i128:
			case Keyword::u8:
			case Keyword::u16:
			case Keyword::u32:
			case Keyword::u64:
			case Keyword::u128:
			case Keyword::f32:
			case Keyword::f64:
			case Keyword::f80:
			case Keyword::Char:
			case Keyword::Bool:
			case Keyword::Vec:
			case Keyword::Set:
			case Keyword::Dict:
			case Keyword::Array:
				is_type_expr = true;
				break;
			default:
				throw base::LogicError("KeywordExpr not yet handled by HoutIsTypeExprVisitor");
			}
		}

		void visitTupleConstructorExpr(const TupleConstructorExpr& tuple) override {
			iterOverExprs(tuple.elements);
		}

		void visitVariantConstructorExpr(const VariantConstructorExpr& variant) override {
			iterOverExprs(variant.subtypes);
		}

		void visitLinkedIdentifierExpr(const LinkedIdentifierExpr& val) override {
			testSymbol(val.symbols.back());
		}

	private:
		void testSymbol(SymID symbol) {
			auto type = *ctx.query<QueryTypeOfSymbol>(symbol);
			if (type.hasError()) {
				// this is a class?
				is_type_expr = true;
			} else {
				switch (type.value().getKind()) {
				case tsh::Kind::Meta:
					is_type_expr = true;
					break;
				default:
					is_type_expr = false;
				}
			}
		}

		void iterOverExprs(const std::vector<base::Box<Expr>>& expressions) {
			is_type_expr = true;
			for (auto& el: expressions) {
				HoutIsTypeExprVisitor vis(ctx, scope);
				el->acceptVisitor(vis);
				if (!vis.is_type_expr) is_type_expr = false;
			}
		}
	};

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
				HoutIsTypeExprVisitor lhs_vis_expr(ctx, scope);
				HoutIsTypeExprVisitor rhs_vis_expr(ctx, scope);
				lhs.node.value()->acceptVisitor(lhs_vis_expr);
				rhs.node.value()->acceptVisitor(rhs_vis_expr);

				if (lhs_vis_expr.is_type_expr != rhs_vis_expr.is_type_expr) {
					// @TODO: Report an error
					return;
				}
				// @TODO: should this not be .value == "|"?
				if (stmt.getOperator().str()[0] == '|' && lhs_vis_expr.is_type_expr
				    && rhs_vis_expr.is_type_expr) {
					node = constructVariantFrom(
						ctx, scope, std::move(*lhs.node), std::move(*rhs.node)
					);
				} else {
					node = makeBox<BinaryOperatorExpr>(
						ctx, scope, stmt.getOperator(), std::move(*lhs.node), std::move(*rhs.node)
					);
				}
			}
		}

		void visitChainExpr(const pst::expr::ChainExpr& stmt) override {
			// @TODO: Add a compiler log or some kind of information if lookup fails.

			auto literal_expr = Expr::fromPST(ctx, scope, stmt.getLiteral());
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
				CORE_ASSERT(pst_access->getType() == ".", "Not handling .? access operator yet");

				std::cout << "Lookup in: " << name(looked_up_symbol.back()).strView() << " "
						  << pst_access->getName().value.strView() << "\n";
				auto new_symbols = *ctx.query<QueryLookupInSymbol>(
					{ looked_up_symbol.back(), pst_access->getName().value, true }
				);

				auto new_symbols_single = new_symbols.getAsSingle();
				CORE_ASSERT(
					new_symbols_single.hasValue(), "Access failed because couldn\'t getAsSingle()"
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
				= *ctx.query<QueryLookupInScopeAndParents>({ scope, stmt.getName().value, true });

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
				node
					= makeBox<UnaryOperatorExpr>(scope, stmt.getOperator(), false, std::move(expr));
			}
		}

		void visitPrefixOperator(const pst::expr::PrefixOperator& stmt) override {
			// @NOTE: This is a mockup
			PstExprToHoutExprVisitor vis(ctx, scope);
			stmt.getExpr()->acceptVisitor(vis);
			if_opt_some(vis.node, expr) {
				node = makeBox<UnaryOperatorExpr>(scope, stmt.getOperator(), true, std::move(expr));
			}
		}
	};

	errors::HResult<base::Box<Expr>, errors::Failed>
		Expr::fromPST(query::Context& ctx, ScopeID scope, const MCRef<pst::ExprElement> root) {
		std::cerr << "\nExpr: \n";
		root->debugPrint(std::cerr);
		std::cerr << '\n';

		PstExprToHoutExprVisitor visitor(ctx, scope);
		root->acceptVisitor(visitor);

		if_opt_some(visitor.node, expr) return std::move(expr);
		return errors::HError(errors::Failed());
	}
}
