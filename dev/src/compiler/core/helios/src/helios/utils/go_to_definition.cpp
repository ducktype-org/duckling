
#include "go_to_definition.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {

	using namespace code;

	/**
	 * @brief Tries to extract a resulting symbol from hout expression.
	 */
	struct HoutResultingSymbolListVisitor final: public HoutExprVisitorEmpty {
		explicit HoutResultingSymbolListVisitor(query::Context& ctx): ctx(ctx) {}

		query::Context& ctx;

		base::Optional<SymID> symbol;

		void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override {
			// note: it should be possible if given operator points to a
			// user defined operator.
			throw base::NotYetImplemented("Cannot evaluate symbol after binary operators");
		}

		void visitIdentifierExpr(const IdentifierExpr& val) override { symbol = val.symbol; }

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {
			// note: it should be possible if given operator points to a
			// user defined operator.
			throw base::NotYetImplemented("Cannot evaluate symbol after unary operators");
		}

		void visitLiteralTypeExpr(const LiteralTypeExpr&) override {
			// @TODO ZPP 3.3 -- make this functionality work on more then just sym ids.
		}

		void visitCallExpr(const CallExpr& call) override { call.callee->acceptVisitor(*this); }

		void visitSequenceExpr(const SequenceExpr& seq) override {
			if (seq.expressions.empty()) return;

			auto                           last = seq.expressions.back().ref();
			HoutResultingSymbolListVisitor vis(ctx);
			last->acceptVisitor(vis);
			symbol = vis.symbol;
		}
	};

	base::Optional<SymID> querySymIDOfHOUTExpr(query::Context& ctx, CRef<code::Expr> expr) {
		HoutResultingSymbolListVisitor visitor(ctx);
		expr->acceptVisitor(visitor);
		return visitor.symbol;
	}

	base::Optional<SymID> querySymIDOfPSTExpr(
		query::Context& ctx, pst::AccessLocked<pst::ExprElement> expr
	) {
		auto hout_expr = ctx.query<compiler::helios::QueryHoutOfExpr>(expr);
		if (!hout_expr->hasValue()) return {};
		return querySymIDOfHOUTExpr(ctx, hout_expr->valueOrThrow().ref());
	}
}
