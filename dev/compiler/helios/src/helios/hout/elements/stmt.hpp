#pragma once

#include "code_block.hpp"
#include "expr.hpp"
#include "../../scope_symbol_id.hpp"

#include <base/ints.hpp>
#include <base/box.hpp>
#include <typesystem/higher/type_desc.hpp>

namespace compiler::helios::code {
	class HoutStmtVisitor;

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		ScopeID lifetime_scope;

		Stmt(ScopeID lifetime_scope): lifetime_scope(lifetime_scope) {}

		virtual ~Stmt()                                                    = default;
		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;

		virtual void acceptVisitor(HoutStmtVisitor&) const = 0;
	};

	/***********************\
	|    DERIVED CLASSES    |
	\***********************/

	/**
	 * @brief Represents `var/let a : T = ..;` statement in HOUT
	 */
	struct VariableStmt final: public Stmt {
		// @TODO: decide where we handle non-initial value (pre hout/post hout):
		// currently PST always have it.
		base::Optional<base::Box<Expr>> initial_value;
		tsh::TypeDesc<>                 type;

		// @TODO decide if this is needed:
		SymID helios_symbol;

		VariableStmt(
			ScopeID                         scope,
			base::Optional<base::Box<Expr>> initial_value,
			tsh::TypeDesc<>                 type,
			SymID                           helios_symbol
		):
			  Stmt(scope),
			  initial_value(std::move(initial_value)),
			  type(type),
			  helios_symbol(helios_symbol) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return [expr];` in HOUT
	 */
	struct ReturnStmt final: public Stmt {
		base::Box<Expr> value;

		ReturnStmt(ScopeID scope, base::Box<Expr> value): Stmt(scope), value(std::move(value)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		VoidReturnStmt(ScopeID scope): Stmt(scope) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		base::Box<Expr> expr;

		ExprStmt(ScopeID scope, base::Box<Expr> expr): Stmt(scope), expr(std::move(expr)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents if statement in HOUT
	 */
	struct IfStmt final: public Stmt {
		base::Box<Expr> condition;
		CodeBlock       body;

		// @TODO: optional else body

		IfStmt(ScopeID scope, base::Box<Expr> condition, CodeBlock body):
			  Stmt(scope),
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};
}
