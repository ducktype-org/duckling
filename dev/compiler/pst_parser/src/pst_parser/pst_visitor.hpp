#pragma once

#include "elements/elements_list.hpp"
#include "pst_parser/access.hpp"
#include <base/exceptions.hpp>

namespace pst {
	// Attention. All of the following [[maybe_unused]] attributes serve purpose of allowing
	// IDEs to generate correct skeleton for defining these methods with a name in place .

	/**
	 * PstVisitor is a simple base class for VisitorPattern in PST.
	 * It does not define visit methods for all elements, rather
	 * it mostly defines it for `pst::Stmt`s and some other needed
	 * by HELIOS.
	 * It is used by calling `element.acceptVisitor(visitor)`.
	 * @note Don't use/implement it for expressions, use PstExprVisitor instead.
	 */
	class PstVisitor {
	public:
		virtual ~PstVisitor() = default;

		virtual void visitImport([[maybe_unused]] Access<Import> stmt) = 0;

		virtual void visitUsing([[maybe_unused]] Access<Using> stmt) = 0;

		virtual void visitAlias([[maybe_unused]] Access<Alias> stmt) = 0;

		virtual void visitExprStmt([[maybe_unused]] Access<ExprStmt> stmt) = 0;

		virtual void visitReturn([[maybe_unused]] Access<Return> stmt) = 0;

		virtual void visitDefer([[maybe_unused]] Access<Defer> stmt) = 0;

		virtual void visitRestart([[maybe_unused]] Access<Restart> stmt) = 0;

		virtual void visitBreak([[maybe_unused]] Access<Break> stmt) = 0;

		virtual void visitContinue([[maybe_unused]] Access<Continue> stmt) = 0;

		virtual void visitRedo([[maybe_unused]] Access<Redo> stmt) = 0;

		virtual void visitThrow([[maybe_unused]] Access<Throw> stmt) = 0;

		virtual void visitConst([[maybe_unused]] Access<Const> stmt) = 0;

		virtual void visitBlock([[maybe_unused]] Access<Block> stmt) = 0;

		virtual void visitNamespace([[maybe_unused]] Access<Namespace> stmt) = 0;

		virtual void visitClass([[maybe_unused]] Access<Class> stmt) = 0;

		virtual void visitFun([[maybe_unused]] Access<Fun> stmt) = 0;

		virtual void visitVariable([[maybe_unused]] Access<Variable> stmt) = 0;

		virtual void visitIf([[maybe_unused]] Access<If> stmt) = 0;

		virtual void visitWhile([[maybe_unused]] Access<While> stmt) = 0;

		virtual void visitMethod([[maybe_unused]] Access<Method> stmt) = 0;

		virtual void visitField([[maybe_unused]] Access<Field> stmt) = 0;

		virtual void visitConstructor([[maybe_unused]] Access<Constructor> stmt) = 0;

		virtual void visitDestructor([[maybe_unused]] Access<Destructor> stmt) = 0;

		virtual void visitAccessBlock([[maybe_unused]] Access<AccessBlock> stmt) = 0;

		virtual void visitFor([[maybe_unused]] Access<For> stmt) = 0;

		virtual void visitFunParam([[maybe_unused]] Access<FunParam> param) = 0;
	};

	/**
	 * A simple implementation for PstVisitor, that by default does nothing on visiting.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstVisitorEmpty: public PstVisitor {
	public:
		~PstVisitorEmpty() override = default;

		void visitImport([[maybe_unused]] Access<Import> stmt) override {}

		void visitUsing([[maybe_unused]] Access<Using> stmt) override {}

		void visitAlias([[maybe_unused]] Access<Alias> stmt) override {}

		void visitExprStmt([[maybe_unused]] Access<ExprStmt> stmt) override {}

		void visitReturn([[maybe_unused]] Access<Return> stmt) override {}

		void visitDefer([[maybe_unused]] Access<Defer> stmt) override {}

		void visitRestart([[maybe_unused]] Access<Restart> stmt) override {}

		void visitBreak([[maybe_unused]] Access<Break> stmt) override {}

		void visitContinue([[maybe_unused]] Access<Continue> stmt) override {}

		void visitRedo([[maybe_unused]] Access<Redo> stmt) override {}

		void visitThrow([[maybe_unused]] Access<Throw> stmt) override {}

		void visitConst([[maybe_unused]] Access<Const> stmt) override {}

		void visitBlock([[maybe_unused]] Access<Block> stmt) override {}

		void visitNamespace([[maybe_unused]] Access<Namespace> stmt) override {}

		void visitClass([[maybe_unused]] Access<Class> stmt) override {}

		void visitFun([[maybe_unused]] Access<Fun> stmt) override {}

		void visitFor([[maybe_unused]] Access<For> stmt) override {}

		void visitVariable([[maybe_unused]] Access<Variable> stmt) override {}

		void visitIf([[maybe_unused]] Access<If> stmt) override {}

		void visitWhile([[maybe_unused]] Access<While> stmt) override {}

		void visitMethod([[maybe_unused]] Access<Method> stmt) override {}

		void visitField([[maybe_unused]] Access<Field> stmt) override {}

		void visitConstructor([[maybe_unused]] Access<Constructor> stmt) override {}

		void visitDestructor([[maybe_unused]] Access<Destructor> stmt) override {}

		void visitAccessBlock([[maybe_unused]] Access<AccessBlock> stmt) override {}

		void visitFunParam([[maybe_unused]] Access<FunParam> param) override {}
	};

/**
 * @brief Macro used to define PstVisitor methods
 */
#define PANIC_VISITOR_VISIT_METHOD(type)                                \
	void visit##type([[maybe_unused]] Access<type> pst_node) override { \
		CORE_PANIC("PstVisitorPanicky visited " #type);                 \
	}

	/**
	 * A simple implementation for PstVisitor, that by default does CORE_PANIC.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstVisitorPanicky: public PstVisitor {
	public:
		~PstVisitorPanicky() override = default;

		PANIC_VISITOR_VISIT_METHOD(Import);
		PANIC_VISITOR_VISIT_METHOD(Using);
		PANIC_VISITOR_VISIT_METHOD(Alias);
		PANIC_VISITOR_VISIT_METHOD(ExprStmt);
		PANIC_VISITOR_VISIT_METHOD(Return);
		PANIC_VISITOR_VISIT_METHOD(Defer);
		PANIC_VISITOR_VISIT_METHOD(Restart);
		PANIC_VISITOR_VISIT_METHOD(Break);
		PANIC_VISITOR_VISIT_METHOD(Redo);
		PANIC_VISITOR_VISIT_METHOD(Continue);
		PANIC_VISITOR_VISIT_METHOD(Throw);
		PANIC_VISITOR_VISIT_METHOD(Const);
		PANIC_VISITOR_VISIT_METHOD(Block);
		PANIC_VISITOR_VISIT_METHOD(Namespace);
		PANIC_VISITOR_VISIT_METHOD(Class);
		PANIC_VISITOR_VISIT_METHOD(Fun);
		PANIC_VISITOR_VISIT_METHOD(For);
		PANIC_VISITOR_VISIT_METHOD(Variable);
		PANIC_VISITOR_VISIT_METHOD(If);
		PANIC_VISITOR_VISIT_METHOD(While);
		PANIC_VISITOR_VISIT_METHOD(Method);
		PANIC_VISITOR_VISIT_METHOD(Field);
		PANIC_VISITOR_VISIT_METHOD(Constructor);
		PANIC_VISITOR_VISIT_METHOD(Destructor);
		PANIC_VISITOR_VISIT_METHOD(AccessBlock);
		PANIC_VISITOR_VISIT_METHOD(FunParam);
	};
}
