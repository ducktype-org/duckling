#pragma once
#include "elements/elements.hpp"

namespace pst {
	class PstStmtVisitor {
	public:
		virtual ~PstStmtVisitor() = 0;

		virtual void visitAttribute(const Attribute& stmt) {}

		virtual void visitImport(const Import& stmt) {}

		virtual void visitUsing(const Using& stmt) {}

		virtual void visitAlias(const Alias& stmt) {}

		virtual void visitExpr(const Expr& stmt) {}

		virtual void visitAction(const Action& stmt) {}

		virtual void visitConst(const Const& stmt) {}

		virtual void visitDecl(const Decl& stmt) {}

		virtual void visitBlock(const Block& stmt) {}

		virtual void visitNamespace(const Namespace& stmt) {}

		virtual void visitStruct(const Struct& stmt) {}

		virtual void visitFun(const Fun& stmt) {}
	};
}
