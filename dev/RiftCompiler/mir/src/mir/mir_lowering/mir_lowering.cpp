#include "mir_lowering.hpp"
#include <query_framework/query_impl.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>


namespace compiler::mir {


	// @TODO: HoleID, BlockID, 

	namespace hc = helios::code;

	struct ExprLowerRes {
		// block
		// mir location
	};

	struct StmtLowerRes {
		// block
	};


	struct StmtBlockVisitor: public hc::HoutStmtVisitor {
		base::Optional<StmtLowerRes> out;

		void output(StmtLowerRes value) {
			this->out.emplace(value);
		}

		
		void visitReturnStmt(const hc::ReturnStmt& stmt) override {
			throw base::NotYetImplemented("return");
		}
		void visitVoidReturnStmt(const hc::VoidReturnStmt& stmt) override {
			throw base::NotYetImplemented("v return");
		}
		void visitExprStmt(const hc::ExprStmt& stmt) override {
			throw base::NotYetImplemented("expr");
		}
		void visitIfStmt(const hc::IfStmt& stmt) override {
			throw base::NotYetImplemented("if");
		}
	};

	struct ExprBlockVisitor: public helios::code::HoutExprVisitor {
		base::Optional<ExprLowerRes> out;

		void output(ExprLowerRes value) {
			this->out.emplace(value);
		}

		void visitConstIntExprMock(const hc::ConstIntExprMock& expr) override {
			throw base::NotYetImplemented("expr");
		}
		void visitIdentifierExpresion(const hc::IdentifierExpresion& expr) override {
			throw base::NotYetImplemented("identifier");
		}
	};

	StmtLowerRes lowerStmt(base::borrow_ptr<hc::Stmt> stmt) {
		StmtBlockVisitor visitor;
		stmt->acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes lowerExpr(base::borrow_ptr<hc::Expr> expr) {
		ExprBlockVisitor visitor;
		expr->acceptVisitor(visitor);
		return visitor.out.value();
	}

	// @TODO: StmtExprBoolJmpVisitor for jumping code

	struct IMPLEMENT_QUERY(LowerToMirFunction, Function) {
		auto provide(Context& ctx, QKey key) {
			// @TODO:
			// * build cfg+quad step by step
			// * add some lifetime stuff
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);



}