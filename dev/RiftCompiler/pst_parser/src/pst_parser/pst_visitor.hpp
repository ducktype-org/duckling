#pragma once
#include "elements/elements.hpp"

namespace pst {
	// Attention. All of the following [[maybe_unused]] attributes serve purpose of allowing
	// IDEs to generate correct skeleton for defining these methods with a name in place .


	/**
	 * PstStmtVisitor is a simple base class for VisitorPattern in `pst::Stmt`s.
	 * It is used by calling `stmt.acceptVisitor(visitor)`.
	 */
	class PstStmtVisitor {
	public:
		virtual ~PstStmtVisitor() = default;

		virtual void visitAttribute([[maybe_unused]] const Attribute& stmt) = 0;

		virtual void visitImport([[maybe_unused]] const Import& stmt) = 0;

		virtual void visitUsing([[maybe_unused]] const Using& stmt) = 0;

		virtual void visitAlias([[maybe_unused]] const Alias& stmt) = 0;

		virtual void visitExpr([[maybe_unused]] const Expr& stmt) = 0;

		virtual void visitAction([[maybe_unused]] const Action& stmt) = 0;

		virtual void visitConst([[maybe_unused]] const Const& stmt) = 0;

		virtual void visitDecl([[maybe_unused]] const Decl& stmt) = 0;

		virtual void visitBlock([[maybe_unused]] const Block& stmt) = 0;

		virtual void visitNamespace([[maybe_unused]] const Namespace& stmt) = 0;

		virtual void visitStruct([[maybe_unused]] const Struct& stmt) = 0;

		virtual void visitFun([[maybe_unused]] const Fun& stmt) = 0;

		virtual void visitVariable([[maybe_unused]] const Variable& stmt) = 0;
	};

	/**
	 * A simple implementation for PstStmtVisitor, that by default does nothing on visiting.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstStmtVisitorEmpty: public PstStmtVisitor {
	public:
		~PstStmtVisitorEmpty() override = default;

		void visitAttribute([[maybe_unused]] const Attribute& stmt) override {}

		void visitImport([[maybe_unused]] const Import& stmt) override {}

		void visitUsing([[maybe_unused]] const Using& stmt) override {}

		void visitAlias([[maybe_unused]] const Alias& stmt) override {}

		void visitExpr([[maybe_unused]] const Expr& stmt) override {}

		void visitAction([[maybe_unused]] const Action& stmt) override {}

		void visitConst([[maybe_unused]] const Const& stmt) override {}

		void visitDecl([[maybe_unused]] const Decl& stmt) override {}

		void visitBlock([[maybe_unused]] const Block& stmt) override {}

		void visitNamespace([[maybe_unused]] const Namespace& stmt) override {}

		void visitStruct([[maybe_unused]] const Struct& stmt) override {}

		void visitFun([[maybe_unused]] const Fun& stmt) override {}

		void visitVariable([[maybe_unused]] const Variable& stmt) override {}
	};

	/**
	 * A simple implementation for PstStmtVisitor, that by default does RIFT_PANIC.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstStmtVisitorPanicky: public PstStmtVisitor {
	public:
		~PstStmtVisitorPanicky() override = default;

		void visitAttribute([[maybe_unused]] const Attribute& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Attribute");
		}

		void visitImport([[maybe_unused]] const Import& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Import");
		}

		void visitUsing([[maybe_unused]] const Using& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Using");
		}

		void visitAlias([[maybe_unused]] const Alias& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Alias");
		}

		void visitExpr([[maybe_unused]] const Expr& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Expr");
		}

		void visitAction([[maybe_unused]] const Action& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Action");
		}

		void visitConst([[maybe_unused]] const Const& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Const");
		}

		void visitDecl([[maybe_unused]] const Decl& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Decl");
		}

		void visitBlock([[maybe_unused]] const Block& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Block");
		}

		void visitNamespace([[maybe_unused]] const Namespace& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Namespace");
		}

		void visitStruct([[maybe_unused]] const Struct& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Struct");
		}

		void visitFun([[maybe_unused]] const Fun& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Fun");
		}

		void visitVariable([[maybe_unused]] const Variable& stmt) override {
			RIFT_PANIC("PstStmtVisitorPanicky visited Variable");
		}
	};
}
