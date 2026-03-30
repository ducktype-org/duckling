#pragma once

#include "expr.hpp"

#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <vector>

namespace compiler::helios::defgen {
	struct ImplementationOf_QueryImplicitClassConstructor;
}

namespace compiler::helios::code {
	class HoutStmtVisitor;

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		Stmt(ElementOrigin origin): origin(origin) {}

		ElementOrigin origin;

		virtual ~Stmt()                                                    = default;
		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;

		virtual void acceptVisitor(HoutStmtVisitor&) const = 0;

		[[nodiscard]] base::Optional<pst::StablePosition> getPosition() const {
			return origin.getStablePosition();
		}
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
		ElementOrigin             origin;
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
			ElementOrigin           origin,
			Box<Expr>               initial_value,
			const tsh::SymbolType<> type,
			const SymID             helios_symbol
		):
			  Stmt(origin),
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

		AssignmentStmt(ElementOrigin origin, Box<Expr> location_expr, Box<Expr> new_value):
			  Stmt(origin),
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

		ReturnStmt(ElementOrigin origin, Box<Expr> value): Stmt(origin), value(std::move(value)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		VoidReturnStmt(ElementOrigin origin): Stmt(origin) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		Box<Expr> expr;

		ExprStmt(ElementOrigin origin, Box<Expr> expr): Stmt(origin), expr(std::move(expr)) {}

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

		IfStmt(ElementOrigin origin, Box<Expr> condition, CodeBlock then_body, CodeBlock else_body):
			  Stmt(origin),
			  condition(std::move(condition)),
			  then_body(std::move(then_body)),
			  else_body(std::move(else_body)) {}

		IfStmt(ElementOrigin origin, Box<Expr> condition, CodeBlock then_body):
			  Stmt(origin),
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

		WhileStmt(ElementOrigin origin, Box<Expr> condition, CodeBlock body):
			  Stmt(origin),
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};
}
