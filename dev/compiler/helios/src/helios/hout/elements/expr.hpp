#pragma once

#include "../../symbols/symbols.hpp"  // @todo ... #404
#include "../../scope_symbol_id.hpp"

#include <vector>

#include <base/ints.hpp>
#include <base/box.hpp>

#include <query_framework/query_int.hpp>
#include <typesystem/higher/type_desc.hpp>
#include <typesystem/higher/queries.hpp>
#include <helios/lookup_result.hpp>

namespace compiler::helios::code {
	class HoutExprVisitor;

	/**
	 * @brief Base class for all HOUT expressions.
	 * All subclasses shall have a "Expr" suffix.
	 */
	struct Expr {
		ScopeID lifetime_scope;

		/**
		 * The type of the expression, and its value category.
		 */
		tsh::TypeDesc<> type_desc;

		Expr(ScopeID lifetime_scope, tsh::TypeDesc<> type_desc):
			  lifetime_scope(lifetime_scope),
			  type_desc(type_desc) {}

		virtual ~Expr()                                  = default;
		virtual void debugPrint(std::ostream& out) const = 0;

		virtual void acceptVisitor(HoutExprVisitor&) const = 0;
	};

	/***********************\
	|    DERIVED CLASSES    |
	\***********************/

	/**
	 * @brief Represents an integer literal value written in the expression.
	 */
	struct LiteralIntExpr final: public Expr {
		// @TODO: ctv + type for consts?
		// @note: this is a mock
		i64 value;

		LiteralIntExpr(query::Context& ctx, ScopeID scope, i64 value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a boolean literal value written in the expression (true, false).
	 */
	struct LiteralBoolExpr final: public Expr {
		bool value;

		LiteralBoolExpr(query::Context& ctx, ScopeID scope, bool value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a type literal value written in the expression (e.g. i32, i64, bool, void).
	 */
	struct LiteralTypeExpr final: public Expr {
		tsh::AbstractType value_type;

		LiteralTypeExpr(query::Context& ctx, ScopeID scope, tsh::AbstractType type);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents expression made of a single identifier in HOUT.
	 * @note: This will have to be improved,
	 * when more complex expressions involving "." operator, local variables, etc
	 * will be introduced.
	 */
	struct IdentifierExpr final: public Expr {
		// @note: this is a mock
		SymID symbol;

		IdentifierExpr(query::Context& ctx, ScopeID scope, SymID symbol);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents an expression inside "(" and ")".
	 * @TODO: Decide if this class is needed.
	 * For:
	 * - nice dprints, because with this class we know what was in "()"
	 * Against:
	 * - We have/will have TupleTypeConstructorExpr and VariantConstructor Expr.
	 *
	 * @TODO HOUT 2.0: once variants are chained in PST we can delete it
	 * For now it will be kept for simplicity of creating VariantTypeConstructorExpr.
	 * Also we should print all "()" from hout structure anyway.
	 */
	struct ParenthesisExpr final: public Expr {
		base::Box<Expr> inner;

		ParenthesisExpr(query::Context& ctx, ScopeID scope, base::Box<Expr> inner);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * Builtin binary operation.
	 */
	enum class BuiltinBinary {
		// we don't have to be super specific here
		// we will likely want to be super specific in LIR

		IntegerAdd,
		IntegerSub,
		IntegerMul,
		IntegerDiv,
		IntegerMod,
		IntegerPow,

		IntegerLt,  //< Less than
	};

	/**
	 * @brief A binary operator.
	 */
	struct BinaryOperatorExpr: public Expr {
		BuiltinBinary operation;

		base::Box<Expr> lhs;
		base::Box<Expr> rhs;

		BinaryOperatorExpr(
			query::Context& ctx,
			ScopeID         scope,
			BuiltinBinary   operation,
			base::Box<Expr> lhs,
			base::Box<Expr> rhs
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Builtin unary operations.
	 */
	enum class BuiltinUnary {
		// we don't have to be super specific here
		// we will likely want to be super specific in LIR

		IntegerNegation,
	};

	/**
	 * @brief General unary operator. Correctness depends on a proper lookup of a method (operator).
	 */
	struct UnaryOperatorExpr: public Expr {
		BuiltinUnary operation;

		base::Box<Expr> expr;

		UnaryOperatorExpr(ScopeID scope, BuiltinUnary operation, base::Box<Expr> expr);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Tuple constructor inside an expression.
	 */
	struct TupleTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> elements;

		TupleTypeConstructorExpr(
			query::Context& ctx, ScopeID scope, std::vector<base::Box<Expr>> elements
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Variant constructor inside an expression.
	 */
	struct VariantTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> subtypes;

		VariantTypeConstructorExpr(
			query::Context& ctx, ScopeID scope, std::vector<base::Box<Expr>> subtypes
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents the "IDENTIFIER.DATA[.DATA]*" format of SymbolList.
	 * @NOTE Currently it is just a mockup. Should be refactored to AccessExpr
	 * -----
	 * @TODO: We should implement shortening of the SymbolList, ex. leave only
	 * the "IDENTIFIER.DATA[.DATA]*" format of SymbolList
	 # and represent it as a Access/Call tree.
	 */
	struct LinkedIdentifierExpr: public Expr {
		SymbolList symbols;

		LinkedIdentifierExpr(query::Context& ctx, ScopeID scope, SymbolList symbols);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	// @todo HOUT 2.0: function call expression
	// it should hold SymID of a function and vector of arguments
}
