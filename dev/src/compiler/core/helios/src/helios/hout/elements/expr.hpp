#pragma once

#include "../../scope_symbol_id.hpp"

#include <typesystem/higher/expression_type.hpp>

#include <base/box.hpp>
#include <base/ints.hpp>

#include <token_parser_core/common_elements.hpp>

#include <vector>

namespace compiler::helios::code {
	class HoutExprVisitor;

#define FRIEND_MAKEBOX                              \
	template<class T, class Deleter, class... Args> \
	friend base::Box<T, Deleter> base::makeBox(Args&&... args);

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

		virtual ~Expr() = default;

		virtual void debugPrint(std::ostream& out) const = 0;

		virtual void acceptVisitor(HoutExprVisitor&) const = 0;

		/**
		 * @brief Deep copy of the expression tree.
		 * It was needed for the function default argument functionality,
		 * to copy the default argument into every call site.
		 * Use with caution.
		 * @return Box<Expr> ownership of the copy of the expression.
		 */
		[[nodiscard]] virtual Box<Expr> clone() const = 0;
	};

	/***********************\
	|    DERIVED CLASSES    |
	\***********************/

	/**
	 * @brief Represents a unit literal value: `()`.
	 *
	 * It acts as both a value and a type. By default, it is interpreted
	 * as a value, but it is lazily lifted to a type if necessary.
	 */
	struct LiteralUnitExpr final: public Expr {
		LiteralUnitExpr(query::Context& ctx);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const override;

	private:
		FRIEND_MAKEBOX

		LiteralUnitExpr(tsh::ExpressionType<> expression_type);
	};

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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralIntExpr(tsh::ExpressionType<> expression_type, i64 value);
	};

	/**
	 * @brief Represents a boolean literal value written in the expression (true, false).
	 */
	struct LiteralBoolExpr final: public Expr {
		bool value;

		LiteralBoolExpr(query::Context& ctx, bool value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralBoolExpr(tsh::ExpressionType<> expression_type, bool value);
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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralStringExpr(tsh::ExpressionType<> expression_type, tpc::StringValue value);
	};

	/**
	 * @brief Represents a type literal value written in the expression (e.g. i32, i64, bool, void).
	 */
	struct LiteralTypeExpr final: public Expr {
		tsh::SymbolType<> value_type;

		LiteralTypeExpr(query::Context& ctx, tsh::AbstractType type);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralTypeExpr(tsh::ExpressionType<> expression_type, tsh::SymbolType<> value_type);
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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		IdentifierExpr(tsh::ExpressionType<> expression_type, SymID symbol);
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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		ParenthesisExpr(tsh::ExpressionType<> expression_type, base::Box<Expr> inner);
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

		// Comparison operators
		IntegerLt,    // Less then
		IntegerLteq,  // Less then or equal to
		IntegerGt,    // Greater then
		IntegerGteq,  // Greater then or equal to
		IntegerEq,    // Equal
		IntegerNeq,   // Not equal

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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		BinaryOperatorExpr(
			tsh::ExpressionType<> expression_type,
			BuiltinBinary         operation,
			base::Box<Expr>       lhs,
			base::Box<Expr>       rhs
		);
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
		Const,
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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		UnaryOperatorExpr(
			tsh::ExpressionType<> expression_type, BuiltinUnary operation, base::Box<Expr> expr
		);
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

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		TernaryOperatorExpr(
			tsh::ExpressionType<> expression_type,
			Box<Expr>             condition,
			Box<Expr>             if_true,
			Box<Expr>             if_false
		);
	};

	/**
	 * @brief Tuple constructor inside an expression.
	 */
	struct TupleTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> elements;

		TupleTypeConstructorExpr(query::Context& ctx, std::vector<base::Box<Expr>> elements);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		TupleTypeConstructorExpr(
			tsh::ExpressionType<> expression_type, std::vector<base::Box<Expr>> elements
		);
	};

	/**
	 * @brief Variant constructor inside an expression.
	 */
	struct VariantTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> subtypes;

		VariantTypeConstructorExpr(query::Context& ctx, std::vector<base::Box<Expr>> subtypes);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		VariantTypeConstructorExpr(
			tsh::ExpressionType<> expression_type, std::vector<base::Box<Expr>> subtypes
		);
	};

	/**
	 * @brief Represents a field access to an expression, like "some_struct.field".
	 * For now it is a mockup, doesn't work.
	 */
	struct AccessExpr final: public Expr {
		Box<Expr>   base;
		base::StrID field;

		AccessExpr(query::Context& ctx, base::Box<Expr> base, base::StrID field);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		AccessExpr(tsh::ExpressionType<> expression_type, base::Box<Expr> base, base::StrID field);
	};

	/**
	 * @brief Represents a call in an expression.
	 */
	struct CallExpr final: public Expr {
		base::Box<Expr>              callee;
		std::vector<base::Box<Expr>> arguments;

		CallExpr(query::Context& ctx, base::Box<Expr> callee, std::vector<base::Box<Expr>> arguments);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		CallExpr(
			tsh::ExpressionType<>        expression_type,
			base::Box<Expr>              callee,
			std::vector<base::Box<Expr>> arguments
		);
	};

	/**
	 * @brief Represent a sequence of expressions, like "a, b, c;".
	 * The results of all the expressions are discarded, except for the last one.
	 * The last one is also the result of the whole sequence.
	 *
	 * Acts like a comma operator in C/C++:
	 * > the comma operator is a binary operator that evaluates its first operand and discards
	 * the result, and then evaluates the second operand and returns this value
	 */
	struct SequenceExpr final: public Expr {
		std::vector<base::Box<Expr>> expressions;

		SequenceExpr(query::Context& ctx, std::vector<base::Box<Expr>> expressions);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		SequenceExpr(tsh::ExpressionType<> expression_type, std::vector<base::Box<Expr>> expressions);
	};

	/**
	 * @brief Represents a chain comparison, like "a < b == c >= d"
	 * The result is a logical and of each separate comparison.
	 * @TODO: User defined comparison operators.
	 */
	struct ChainComparisonExpr final: public Expr {
		std::vector<base::Box<Expr>> expressions;
		std::vector<BuiltinBinary>   operators;

		ChainComparisonExpr(
			query::Context&              ctx,
			std::vector<base::Box<Expr>> expressions,
			std::vector<BuiltinBinary>   operators
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		ChainComparisonExpr(
			tsh::ExpressionType<>        expression_type,
			std::vector<base::Box<Expr>> expressions,
			std::vector<BuiltinBinary>   operators
		);
	};
}
