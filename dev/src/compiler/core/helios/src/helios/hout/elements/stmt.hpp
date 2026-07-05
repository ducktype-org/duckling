#pragma once

#include "expr.hpp"

#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/pointers/box.hpp>
#include <base/pointers/box_or_ref.hpp>
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

		[[nodiscard]] base::Optional<dia_int::StablePosition> getPosition() const {
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
		base::StrID                     name;
		tsh::SymbolType<>               type;
		base::Optional<BoxOrCRef<Expr>> initial_value;
		SymID                           helios_symbol;
		ElementOrigin                   origin;
	};

	/***********************\
	|    DERIVED CLASSES    |
	\***********************/

	/**
	 * @brief Represents `var/let a : T = ..;` statement in HOUT
	 */
	struct VariableStmt final: public Stmt {
		BoxOrCRef<Expr>   initial_value;
		tsh::SymbolType<> type;

		// @TODO decide if this is needed:
		SymID helios_symbol;

		VariableStmt(
			ElementOrigin           origin,
			BoxOrCRef<Expr>         initial_value,
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
		BoxOrCRef<Expr> location_expr;
		BoxOrCRef<Expr> new_value_expr;

		AssignmentStmt(
			ElementOrigin origin, BoxOrCRef<Expr> location_expr, BoxOrCRef<Expr> new_value
		):
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
		BoxOrCRef<Expr> value;

		ReturnStmt(ElementOrigin origin, BoxOrCRef<Expr> value):
			  Stmt(origin),
			  value(std::move(value)) {}

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
		BoxOrCRef<Expr> expr;

		ExprStmt(ElementOrigin origin, BoxOrCRef<Expr> expr): Stmt(origin), expr(std::move(expr)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents if statement in HOUT
	 */
	struct IfStmt final: public Stmt {
		BoxOrCRef<Expr> condition;
		CodeBlock       then_body;
		CodeBlock       else_body;

		IfStmt(
			ElementOrigin origin, BoxOrCRef<Expr> condition, CodeBlock then_body, CodeBlock else_body
		):
			  Stmt(origin),
			  condition(std::move(condition)),
			  then_body(std::move(then_body)),
			  else_body(std::move(else_body)) {}

		IfStmt(ElementOrigin origin, BoxOrCRef<Expr> condition, CodeBlock then_body):
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
		BoxOrCRef<Expr> condition;
		CodeBlock       body;

		WhileStmt(ElementOrigin origin, BoxOrCRef<Expr> condition, CodeBlock body):
			  Stmt(origin),
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	struct BlockStmt final: public Stmt {
		CodeBlock body;

		BlockStmt(ElementOrigin origin, CodeBlock body): Stmt(origin), body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Lowered `match` over a variant value.
	 *
	 * Cases are tried in order. A case either tests one concrete alternative of the
	 * subject's variant type or is a wildcard (empty alternative index) that always
	 * matches. Pattern bindings are represented as a VariableStmt at the start of the
	 * case body, initialized with a VariantProjectExpr.
	 */
	struct MatchStmt final: public Stmt {
		struct Case final {
			/** Alternative index in the subject's variant type; empty for wildcards. */
			base::Optional<usize> alternative_index;
			CodeBlock             body;
		};

		Box<Expr>         subject;  ///< Variant-typed subject; must be a readable place.
		std::vector<Case> cases;

		MatchStmt(ElementOrigin origin, Box<Expr> subject, std::vector<Case> cases):
			  Stmt(origin),
			  subject(std::move(subject)),
			  cases(std::move(cases)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};
}
