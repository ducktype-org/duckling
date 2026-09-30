#pragma once

#include "expr.hpp"

#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/pointers/box.hpp>
#include <base/pointers/box_or_ref.hpp>
#include <base/types/ints.hpp>

#include <variant>
#include <vector>

namespace compiler::helios::defgen {
	struct ImplementationOf_QueryImplicitClassConstructor;
}

namespace compiler::helios::code {
	class HoutStmtVisitor;

	enum class ControlFlowKind { If, While, For, Block };

	struct NearestLoop {};

	struct NamedTarget {
		SymID id;
	};

	struct KindTarget {
		ControlFlowKind kind;
	};

	using ControlFlowTargetSelector = std::variant<NearestLoop, NamedTarget, KindTarget>;

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		Stmt(ElementOrigin origin): origin(origin) {}

		ElementOrigin origin;

		virtual ~Stmt()                                                    = default;
		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;

		virtual void acceptVisitor(HoutStmtVisitor&) const = 0;

		[[nodiscard]] base::Optional<dia::StablePosition> getPosition() const {
			return origin.getStablePosition();
		}

		[[nodiscard]]
		virtual Box<Stmt> clone() const
			= 0;
	};

	/**
	 * @brief A block (i.e. list) of consecutive HOUT statements
	 */
	struct CodeBlock final {
		std::vector<Box<Stmt>> statements;

		[[nodiscard]] Box<CodeBlock> clone() const;
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

		[[nodiscard]] Box<Parameter> clone() const;
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

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
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

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents `return [expr];` in HOUT
	 */
	struct ReturnStmt final: public Stmt {
		BoxOrCRef<Expr> value;

		ReturnStmt(ElementOrigin origin, BoxOrCRef<Expr> value):
			  Stmt(origin),
			  value(std::move(value)) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		VoidReturnStmt(ElementOrigin origin): Stmt(origin) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		BoxOrCRef<Expr> expr;

		ExprStmt(ElementOrigin origin, BoxOrCRef<Expr> expr): Stmt(origin), expr(std::move(expr)) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents if statement in HOUT
	 */
	struct IfStmt final: public Stmt {
		BoxOrCRef<Expr>       condition;
		CodeBlock             then_body;
		CodeBlock             else_body;
		base::Optional<SymID> control_flow_id;

		IfStmt(
			ElementOrigin         origin,
			BoxOrCRef<Expr>       condition,
			CodeBlock             then_body,
			CodeBlock             else_body,
			base::Optional<SymID> control_flow_id = {}
		):
			  Stmt(origin),
			  condition(std::move(condition)),
			  then_body(std::move(then_body)),
			  else_body(std::move(else_body)),
			  control_flow_id(control_flow_id) {}

		IfStmt(
			ElementOrigin         origin,
			BoxOrCRef<Expr>       condition,
			CodeBlock             then_body,
			base::Optional<SymID> control_flow_id = {}
		):
			  Stmt(origin),
			  condition(std::move(condition)),
			  then_body(std::move(then_body)),
			  else_body({}),
			  control_flow_id(control_flow_id) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents While statement in HOUT
	 */
	struct WhileStmt final: public Stmt {
		BoxOrCRef<Expr>       condition;
		CodeBlock             body;
		base::Optional<SymID> control_flow_id;
		ControlFlowKind       control_flow_kind;

		WhileStmt(
			ElementOrigin         origin,
			BoxOrCRef<Expr>       condition,
			CodeBlock             body,
			base::Optional<SymID> control_flow_id   = {},
			ControlFlowKind       control_flow_kind = ControlFlowKind::While
		):
			  Stmt(origin),
			  condition(std::move(condition)),
			  body(std::move(body)),
			  control_flow_id(control_flow_id),
			  control_flow_kind(control_flow_kind) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents break statement in HOUT
	 */
	struct BreakStmt final: public Stmt {
		ControlFlowTargetSelector target_selector;

		BreakStmt(ElementOrigin origin, ControlFlowTargetSelector target_selector = NearestLoop{}):
			  Stmt(origin),
			  target_selector(target_selector) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	/**
	 * @brief Represents continue statement in HOUT
	 */
	struct ContinueStmt final: public Stmt {
		ControlFlowTargetSelector target_selector;

		ContinueStmt(ElementOrigin origin, ControlFlowTargetSelector target_selector = NearestLoop{}):
			  Stmt(origin),
			  target_selector(target_selector) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

	struct BlockStmt final: public Stmt {
		CodeBlock                       body;
		base::Optional<SymID>           control_flow_id;
		base::Optional<ControlFlowKind> control_flow_kind;

		BlockStmt(
			ElementOrigin                   origin,
			CodeBlock                       body,
			base::Optional<SymID>           control_flow_id   = {},
			base::Optional<ControlFlowKind> control_flow_kind = {}
		):
			  Stmt(origin),
			  body(std::move(body)),
			  control_flow_id(control_flow_id),
			  control_flow_kind(control_flow_kind) {}

		void                    debugPrint(std::ostream& out, usize indent = 0) const final;
		void                    acceptVisitor(HoutStmtVisitor&) const override;
		[[nodiscard]] Box<Stmt> clone() const final;
	};

}
