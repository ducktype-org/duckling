#pragma once

#include <helios/scope_symbol_id.hpp>
#include "expr.hpp"

#include <typesystem/higher/symbol_type.hpp>

#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <vector>

namespace compiler::helios::houtgen {
	struct ImplementationOf_QueryImplicitClassConstructor;
}

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
			Box<Expr> initial_value, const tsh::SymbolType<> type, const SymID helios_symbol
		):
			  initial_value(std::move(initial_value)),
			  type(type),
			  helios_symbol(helios_symbol) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;

	private:
		// This constructor is used when one cannot possibly set an initial
		// value, for example for the result variable of a class's constructor.
		VariableStmt(
			base::Optional<Box<Expr>> initial_value,
			tsh::SymbolType<>         type,
			const SymID               helios_symbol
		):
			  initial_value(std::move(initial_value)),
			  type(type),
			  helios_symbol(helios_symbol) {}

		// Friend for constructing VariableStmt without initial value.
		friend houtgen::ImplementationOf_QueryImplicitClassConstructor;
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
		CodeBlock then_body;
		CodeBlock else_body;

		IfStmt(Box<Expr> condition, CodeBlock then_body, CodeBlock else_body):
			  condition(std::move(condition)),
			  then_body(std::move(then_body)),
			  else_body(std::move(else_body)) {}

		IfStmt(Box<Expr> condition, CodeBlock then_body):
			  condition(std::move(condition)),
			  then_body(std::move(then_body)),
			  else_body({}) {}

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
