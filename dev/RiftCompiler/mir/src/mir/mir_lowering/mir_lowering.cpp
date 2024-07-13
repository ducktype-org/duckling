#include "mir_lowering.hpp"
#include <query_framework/query_impl.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <base/stable_container.hpp>


namespace compiler::mir {

	struct BlockBuilder {

	};

	using BlockRef = base::StableVectorRef<BlockBuilder>;

	struct InstructionHole {
	private:
		BlockRef block_ref;
		usize position;
	public:
		// @TODO: fill, check if filled, etc
	};

	struct FunctionBuilder {
		base::StableVector<BlockBuilder> blocks;
	};

	



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

	StmtLowerRes lowerStmt(const hc::Stmt& stmt) {
		StmtBlockVisitor visitor;
		stmt.acceptVisitor(visitor);
		return visitor.out.value();
	}

	ExprLowerRes lowerExpr(const hc::Expr& expr) {
		ExprBlockVisitor visitor;
		expr.acceptVisitor(visitor);
		return visitor.out.value();
	}

	StmtLowerRes lowerCodeBlock(const hc::CodeBlock& code_block) {
		// @TODO...
		StmtLowerRes last_result;
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result = lowerStmt(*stmt);
		}
		return last_result;
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