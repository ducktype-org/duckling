
#include "go_to_definition.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>

namespace compiler::helios {

	using namespace code;

	/**
	 * @brief Tries to extract a resulting symbol from hout expression.
	 * @note Logic like this might be useful one day for "go-to-definition" on expressions,
	 * but it might get removed from hout creation in the future.
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

		void visitParenthesisExpr(const ParenthesisExpr& val) override {
			HoutResultingSymbolListVisitor vis(ctx);
			val.inner->acceptVisitor(vis);
			symbol = vis.symbol;
		}

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {
			// note: it should be possible if given operator points to a
			// user defined operator.
			throw base::NotYetImplemented("Cannot evaluate symbol after unary operators");
		}

		void visitLinkedIdentifierExpr(const LinkedIdentifierExpr& expr) override {
			// this is very much a mock, LinkedIdentifierExpr should be deleted:
			symbol = expr.symbols.back();
		}

		void visitLiteralTypeExpr(const LiteralTypeExpr&) override {
			//...
		}

		void visitCallExpr(const CallExpr& call) override { symbol = call.callee; }
	};

	base::Optional<SymID> querySymIDOfExpr(query::Context& ctx, CRef<code::Expr> expr) {
		HoutResultingSymbolListVisitor visitor(ctx);
		expr->acceptVisitor(visitor);
		return visitor.symbol;
	}
}
