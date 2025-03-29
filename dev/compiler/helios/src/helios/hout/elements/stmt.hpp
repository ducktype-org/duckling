#pragma once

#include "expr.hpp"
#include "../../scope_symbol_id.hpp"

#include <vector>

#include <base/ints.hpp>
#include <base/box.hpp>
#include <typesystem/higher/expression_type.hpp>

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

	/**
	 * @brief A block of HOUT statements
	 */
	struct CodeBlock final {
		ScopeID lifetime_scope;
		// @TODO: Make sure that this template instantiation with incomplete type Stmt is not UB.
		std::vector<Box<Stmt>> statements;
	};

	/**
	 * @brief Represents HOUT function parameter.
	 */
	struct Parameter final {
		base::StrID               name;
		tsh::SymbolType<>         type;
		base::Optional<Box<Expr>> initial_value;
		SymID                     helios_symbol;
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
		base::Optional<Box<Expr>> initial_value;
		tsh::SymbolType<>         type;

		// @TODO decide if this is needed:
		SymID helios_symbol;

		VariableStmt(
			const ScopeID             scope,
			base::Optional<Box<Expr>> initial_value,
			tsh::SymbolType<>         type,
			const SymID               helios_symbol
		):
			  Stmt(scope),
			  initial_value(std::move(initial_value)),
			  type(type),
			  helios_symbol(helios_symbol) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `a = ..;` statement in HOUT
	 */
	struct AssignmentStmt final: public Stmt {
		// TODO: #469 Support arbitrary lvalues on the left.
		Box<Expr> new_value;
		SymID     helios_symbol;

		AssignmentStmt(const ScopeID scope, Box<Expr> new_value, const SymID helios_symbol):
			  Stmt(scope),
			  new_value(std::move(new_value)),
			  helios_symbol(helios_symbol) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return [expr];` in HOUT
	 */
	struct ReturnStmt final: public Stmt {
		Box<Expr> value;

		ReturnStmt(ScopeID scope, Box<Expr> value): Stmt(scope), value(std::move(value)) {}

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
		Box<Expr> expr;

		ExprStmt(ScopeID scope, Box<Expr> expr): Stmt(scope), expr(std::move(expr)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents if statement in HOUT
	 */
	struct IfStmt final: public Stmt {
		Box<Expr> condition;
		CodeBlock body;

		// @TODO: optional else body

		IfStmt(ScopeID scope, Box<Expr> condition, CodeBlock body):
			  Stmt(scope),
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};
}
