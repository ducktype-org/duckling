#pragma once

#include "../../scope_symbol_id.hpp"
#include "expr.hpp"

#include <typesystem/higher/symbol_type.hpp>

#include <base/box.hpp>
#include <base/ints.hpp>

#include <vector>

namespace compiler::helios::code {
	class HoutStmtVisitor;

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		Stmt() = default;

		virtual ~Stmt()                                                    = default;
		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;

		virtual void acceptVisitor(HoutStmtVisitor&) const = 0;
	};

	/**
	 * @brief A block (i.e. list) of consecutive HOUT statements
	 */
	struct CodeBlock final {
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
		// Right now we allow no-initial value here for testing purposes.
		// In the future either HELIOS or MIR should emit default initialization.
		// For now MIR panics on no initial value.
		base::Optional<Box<Expr>> initial_value;
		tsh::SymbolType<>         type;

		// @TODO decide if this is needed:
		SymID helios_symbol;

		VariableStmt(
			base::Optional<Box<Expr>> initial_value,
			tsh::SymbolType<>         type,
			const SymID               helios_symbol
		):
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
		Box<Expr> location_expr;
		Box<Expr> new_value_expr;

		AssignmentStmt(Box<Expr> location_expr, Box<Expr> new_value):
			  location_expr(std::move(location_expr)),
			  new_value_expr(std::move(new_value)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return [expr];` in HOUT
	 */
	struct ReturnStmt final: public Stmt {
		Box<Expr> value;

		ReturnStmt(Box<Expr> value): value(std::move(value)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		VoidReturnStmt() = default;

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		Box<Expr> expr;

		ExprStmt(Box<Expr> expr): expr(std::move(expr)) {}

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

		IfStmt(Box<Expr> condition, CodeBlock body):
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents While statement in HOUT
	 */
	struct WhileStmt final: public Stmt {
		Box<Expr> condition;
		CodeBlock body;

		WhileStmt(Box<Expr> condition, CodeBlock body):
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};
}
