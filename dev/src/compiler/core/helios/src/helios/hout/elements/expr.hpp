#pragma once

#include "../../scope_symbol_id.hpp"

#include <helios/utils/symbol_list.hpp>
#include <query_framework/query_int.hpp>
#include <token_parser_core/common_elements.hpp>
#include <typesystem/higher/expression_type.hpp>

#include <base/box.hpp>
#include <base/ints.hpp>

#include <vector>

namespace compiler::helios::code {
	class HoutExprVisitor;

	/**
	 * @brief Base class for all HOUT expressions.
	 * All subclasses shall have a "Expr" suffix.
	 */
	struct Expr {
		/**
		 * The type of the expression, and its value category.
		 */
		tsh::ExpressionType<> expression_type;

		Expr(tsh::ExpressionType<> expression_type): expression_type(expression_type) {}

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

		LiteralIntExpr(query::Context& ctx, i64 value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a boolean literal value written in the expression (true, false).
	 */
	struct LiteralBoolExpr final: public Expr {
		bool value;

		LiteralBoolExpr(query::Context& ctx, bool value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a string literal value written in the expression ("Hello world" etc.).
	 */
	struct LiteralStringExpr final: public Expr {
		/**
		 * @note value is a StringValue, not a String.
		 * Thus the character escaping sequences are kept in the value.
		 * Ex. in "Hello world\n" new line character is kept as "\n" not as literal new line.
		 */
		tpc::StringValue value;

		LiteralStringExpr(query::Context& ctx, tpc::StringValue value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a type literal value written in the expression (e.g. i32, i64, bool, void).
	 */
	struct LiteralTypeExpr final: public Expr {
		tsh::SymbolType<> value_type;

		LiteralTypeExpr(query::Context& ctx, tsh::AbstractType type);

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

		IdentifierExpr(query::Context& ctx, SymID symbol);

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

		ParenthesisExpr(query::Context& ctx, base::Box<Expr> inner);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * Builtin binary operation.
	 */
	enum class BuiltinBinary : std::uint8_t {
		// we don't have to be super specific here
		// we will likely want to be super specific in LIR

		IntegerAdd,
		IntegerSub,
		IntegerMul,
		IntegerDiv,
		IntegerMod,
		IntegerPow,

		IntegerLt,  //< Less than

		BooleanAnd,
		BooleanOr,
	};

	/**
	 * @brief A binary operator.
	 */
	struct BinaryOperatorExpr: public Expr {
		BuiltinBinary operation;

		base::Box<Expr> lhs;
		base::Box<Expr> rhs;

		BinaryOperatorExpr(
			query::Context& ctx, BuiltinBinary operation, base::Box<Expr> lhs, base::Box<Expr> rhs
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Builtin unary operations.
	 */
	enum class BuiltinUnary : std::uint8_t {
		// we don't have to be super specific here
		// we will likely want to be super specific in LIR

		IntegerNegation,
		BooleanNot,
		Ref,
		Box,
	};

	/**
	 * @brief General unary operator. Correctness depends on a proper lookup of a method (operator).
	 */
	struct UnaryOperatorExpr: public Expr {
		BuiltinUnary operation;

		base::Box<Expr> expr;

		UnaryOperatorExpr(BuiltinUnary operation, base::Box<Expr> expr);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief A ternary operator.
	 */
	struct TernaryOperatorExpr: public Expr {
		Box<Expr> condition;
		Box<Expr> if_true;
		Box<Expr> if_false;

		TernaryOperatorExpr(
			query::Context& ctx, Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Tuple constructor inside an expression.
	 */
	struct TupleTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> elements;

		TupleTypeConstructorExpr(query::Context& ctx, std::vector<base::Box<Expr>> elements);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Variant constructor inside an expression.
	 */
	struct VariantTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> subtypes;

		VariantTypeConstructorExpr(query::Context& ctx, std::vector<base::Box<Expr>> subtypes);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a field access to an expression, like "some_struct.field".
	 * For now it is a mockup, doesn't work.
	 */
	struct AccessExpr final: public Expr {
		base::Box<Expr> base;
		base::StrID     field;

		AccessExpr(query::Context& ctx, base::Box<Expr> base, base::StrID field);
		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represents a call in an expression.
	 */
	struct CallExpr final: public Expr {
		SymID                        callee;
		std::vector<base::Box<Expr>> arguments;

		CallExpr(query::Context& ctx, SymID callee, std::vector<base::Box<Expr>> arguments);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};

	/**
	 * @brief Represent a sequence of expressions, like "a, b, c;".
	 * The results of all the expressions are discarded, except for the last one.
	 * The last one is also the result of the whole sequence.
	 *
	 * Acts like a comma operator in C/C++:
	 * > the comma operator is a binary operator that evaluates its first operand and discards the
	 * result, and then evaluates the second operand and returns this value
	 */
	struct SequenceExpr final: public Expr {
		std::vector<base::Box<Expr>> expressions;

		SequenceExpr(query::Context& ctx, std::vector<base::Box<Expr>> expressions);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;
	};
}
