// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <ctv/numeric_value.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/expression_type.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>

#include <token_parser_core/common_elements.hpp>

#include <optional>
#include <vector>

namespace compiler::helios::code {
	class HoutExprVisitor;
	struct Stmt;

	/**
	 * @brief A block of statements, defined together with the statements in `stmt.hpp`, which
	 * cannot be included here, as it includes this header itself.
	 */
	struct CodeBlock;

#define FRIEND_MAKEBOX                              \
	template<class T, class Deleter, class... Args> \
	friend base::Box<T, Deleter> base::makeBox(Args&&... args);

	/**
	 * @brief Unique ID for each HOUT Expr element.
	 * It can be used as session-unstable HOUT Expr element hash.
	 */
	STRONG_TYPEDEF_ID(HOUTExprID);

	/**
	 * @brief Base class for all HOUT expressions.
	 * All subclasses shall have a "Expr" suffix.
	 */
	struct Expr {
		/**
		 * The type of the expression, and its value category.
		 */
		tsh::ExpressionType<> expression_type;

		ElementOrigin origin;

		Expr(tsh::ExpressionType<> expression_type, ElementOrigin origin):
			  expression_type(expression_type),
			  origin(origin) {}

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
		[[nodiscard]]
		virtual Box<Expr> clone() const
			= 0;

		[[nodiscard]]
		HOUTExprID getID() const {
			return id;
		}

		[[nodiscard]] base::Optional<dia::StablePosition> getPosition() const {
			return origin.getStablePosition();
		}


	private:
		HOUTExprID id = HOUTExprID::next();
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
		LiteralUnitExpr(query::Context& ctx, ElementOrigin origin);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const override;

	private:
		FRIEND_MAKEBOX

		LiteralUnitExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin);
	};

	/**
	 * @brief Represents an numeric literal value written in the expression.
	 * Stores both integer constants and floating point constants.
	 */
	struct LiteralNumericExpr final: public Expr {
		numeric_value::NumericValue value;

		LiteralNumericExpr(
			query::Context& ctx, ElementOrigin origin, numeric_value::NumericValue value
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralNumericExpr(
			tsh::ExpressionType<>       expression_type,
			ElementOrigin               origin,
			numeric_value::NumericValue value
		);
	};

	/**
	 * @brief Represents a boolean literal value written in the expression (true, false).
	 */
	struct LiteralBoolExpr final: public Expr {
		bool value;

		LiteralBoolExpr(query::Context& ctx, ElementOrigin origin, bool value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralBoolExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin, bool value);
	};

	struct LiteralCharExpr final: public Expr {
		char value;

		LiteralCharExpr(query::Context& ctx, const ElementOrigin& origin, char value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralCharExpr(
			const tsh::ExpressionType<>& expression_type, const ElementOrigin& origin, char value
		);
	};

	/**
	 * @brief Represents a string literal value written in the expression ("Hello world" etc.).
	 */
	struct LiteralStringExpr final: public Expr {
		/**
		 * @note This value contains escape sequences, such as "\n", "\t", etc.
		 */
		base::StrID value;

		LiteralStringExpr(query::Context& ctx, ElementOrigin origin, base::StrID value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralStringExpr(
			const tsh::ExpressionType<>& expression_type, ElementOrigin origin, base::StrID value
		);
	};

	/**
	 * @brief Represents a type literal value written in the expression (e.g. i32, i64, bool, void).
	 */
	struct LiteralTypeExpr final: public Expr {
		tsh::SymbolType<> value_type;

		LiteralTypeExpr(query::Context& ctx, ElementOrigin origin, tsh::AbstractType type);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		LiteralTypeExpr(
			tsh::ExpressionType<> expression_type, ElementOrigin origin, tsh::SymbolType<> value_type
		);
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

		IdentifierExpr(query::Context& ctx, ElementOrigin origin, SymID symbol);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		IdentifierExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin, SymID symbol);
	};

	/**
	 * @brief A special kind of expression which wraps expressions that need to be
	 * used multiple times without recalculating, such as `b` in `a < b < c`.
	 * @note One should be very careful not to create a "next use" ReusableExpr which does not
	 * semantically see the result of the corresponding "first use" ReusableExpr, for example
	 * if they are in different branches of an if expression.
	 *
	 * @warning The inner expression must be either trivially copyable or a `Temporary`. For more
	 * info look in the `ReusableExpr` constructor.
	 */
	struct ReusableExpr final: public Expr {
		SharedBox<Expr> inner;
		bool            first_use;

		ReusableExpr(query::Context& ctx, Box<Expr> inner, bool first_use = true);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		// Clones the expression just as any other expression,
		// which means it performs a *deep* copy. Should not be used
		// for reusing a reusable expression. Use nextUse() for that.
		[[nodiscard]] Box<Expr> clone() const final;

		// Create a ReusableExpression with the same inner expression,
		// but prepared for a re-use.
		[[nodiscard]] Box<ReusableExpr> nextUse() const;

	private:
		FRIEND_MAKEBOX

		ReusableExpr(const SharedBox<Expr>& inner, bool first_use);
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
		IntegerBitAnd,
		IntegerBitOr,
		IntegerBitXor,
		IntegerShl,
		IntegerShr,

		FloatAdd,
		FloatSub,
		FloatMul,
		FloatDiv,
		FloatMod,

		// Comparison operators
		IntegerLt,    // Less than
		IntegerLteq,  // Less than or equal to
		IntegerGt,    // Greater than
		IntegerGteq,  // Greater than or equal to
		IntegerEq,    // Equal
		IntegerNeq,   // Not equal

		FloatLt,      // Less than
		FloatLteq,    // Less than or equal to
		FloatGt,      // Greater than
		FloatGteq,    // Greater than or equal to
		FloatEq,      // Equal
		FloatNeq,     // Not equal

		MetaEq,
		MetaNeq,

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
			query::Context& ctx,
			ElementOrigin   origin,
			BuiltinBinary   operation,
			base::Box<Expr> lhs,
			base::Box<Expr> rhs
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		BinaryOperatorExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
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
		FloatNegation,
		BooleanNot,
		IntegerBitNot,
		Ref,
		Ptr,
		ManyPtr,
		CPtr,
		Slice,
		Box,
		Const,
		SizeOf,
		AlignOf
	};

	/**
	 * @brief General unary operator. Correctness depends on a proper lookup of a method (operator).
	 */
	struct UnaryOperatorExpr: public Expr {
		BuiltinUnary operation;

		base::Box<Expr> expr;

		UnaryOperatorExpr(
			query::Context& ctx, ElementOrigin origin, BuiltinUnary operation, base::Box<Expr> expr
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		UnaryOperatorExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
			BuiltinUnary          operation,
			base::Box<Expr>       expr
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
			query::Context& ctx,
			ElementOrigin   origin,
			Box<Expr>       condition,
			Box<Expr>       if_true,
			Box<Expr>       if_false
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		TernaryOperatorExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
			Box<Expr>             condition,
			Box<Expr>             if_true,
			Box<Expr>             if_false
		);
	};

	/**
	 * @brief Tuple constructor expression. This does not create a tuple type, but a tuple value
	 * (e.g.  `(1, 2)`), that *might* be coerced/lifted to a type in some contexts.
	 */
	struct TupleExpr: public Expr {
		std::vector<base::Box<Expr>> elements;
		SymID                        tuple_ctor_symbol;

		TupleExpr(query::Context& ctx, ElementOrigin origin, std::vector<base::Box<Expr>> elements);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		TupleExpr(
			tsh::ExpressionType<>        expression_type,
			ElementOrigin                origin,
			std::vector<base::Box<Expr>> elements,
			SymID                        tuple_ctor_symbol
		);
	};

	/**
	 * @brief Variant constructor inside an expression.
	 */
	struct VariantTypeConstructorExpr: public Expr {
		std::vector<base::Box<Expr>> subtypes;

		VariantTypeConstructorExpr(
			query::Context& ctx, ElementOrigin origin, std::vector<base::Box<Expr>> subtypes
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		VariantTypeConstructorExpr(
			tsh::ExpressionType<>        expression_type,
			ElementOrigin                origin,
			std::vector<base::Box<Expr>> subtypes
		);
	};

	/**
	 * @brief Constructs a variant value from a value of one of its alternatives.
	 *
	 * Created on implicit coercion to a variant type. The inner expression's type must be
	 * exactly equal to the alternative at `alternative_index`.
	 */
	struct VariantConstructExpr final: public Expr {
		Box<Expr> inner;
		usize     alternative_index;

		VariantConstructExpr(
			query::Context&   ctx,
			ElementOrigin     origin,
			Box<Expr>         inner,
			tsh::SymbolType<> variant_type,
			usize             alternative_index
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		VariantConstructExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
			Box<Expr>             inner,
			usize                 alternative_index
		);
	};

	/**
	 * @brief Lowered `match` over a variant value.
	 *
	 * Cases are tried in order. A case either tests one concrete alternative of the
	 * subject's variant type or is a wildcard (empty alternative index) that always
	 * matches. Every case returns and expression (<result>),
	 * and they all have to be of the same type, which
	 * becomes the type of the whole expression.
	 *
	 * Examples:
	 * `case <sym>: <type> = <result>`
	 *     - has alternative index, constraint type <type>, sym <sym>, result <result>
	 * `case _: <type> = <result>`
	 *     - has alternative index, constraint type <type>, result <result>
	 *     - no sym
	 * `case _ = <result>`
	 *     - has result <result>
	 *     - no alternative index, no constraint type, no sym
	 */
	struct MatchExpr final: public Expr {
		struct Case final {
			/** Alternative index in the subject's variant type; empty for wildcards. */
			base::Optional<usize> alternative_index;
			/** Type of the constraint, if provided. The constraint is not provided when wildcard
			 * pattern (matches everything). */
			base::Optional<tsh::SymbolType<>> constraint_type;
			/** This is a variable that is used by the expression,
			  where the alternative value of the same type as constraint should land.*/
			base::Optional<SymID> binding;
			/** The value this case evaluates to. */
			Box<Expr> result;

			[[nodiscard]] bool shouldBindToTemporary(query::Context& ctx) const;
		};

		/**
		 * @brief A `ref` to the matched variant.
		 *
		 * The lowering evaluates it once for the whole case chain, so a subject with side
		 * effects runs exactly once no matter how many alternatives are tested.
		 */
		Box<Expr>         subject;
		std::vector<Case> cases;

		MatchExpr(
			query::Context& ctx, ElementOrigin origin, Box<Expr> subject, std::vector<Case> cases
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		MatchExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
			Box<Expr>             subject,
			std::vector<Case>     cases
		);
	};

	/**
	 * @brief Represents a field access to an expression, like "some_struct.field".
	 * @note This does not represent namespace-like access, like "some_namespace.some_symbol". It
	 * is reserved for field access, with the field name dealiased, etc., in its most direct form.
	 */
	struct AccessExpr final: public Expr {
		Box<Expr> base;
		SymID     field;

		AccessExpr(query::Context& ctx, ElementOrigin origin, Box<Expr> base, SymID field);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		AccessExpr(
			const tsh::ExpressionType<>& expression_type,
			ElementOrigin                origin,
			Box<Expr>                    base,
			SymID                        field
		);
	};

	/**
	 * @brief Represents an array indexing operation.
	 *
	 * This expression is used in two cases:
	 * - When the base is a list or a static array, the index is expected to be an i64 integer. This
	 * then represents an index access (array[0]).
	 * - When the base is a meta type, the index is expected to be an i64 integer. This then
	 * represents a static array type creation.
	 */
	struct IndexExpr final: public Expr {
		Box<Expr> base;
		Box<Expr> index;

		IndexExpr(query::Context& ctx, ElementOrigin origin, Box<Expr> base, Box<Expr> index);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		IndexExpr(
			const tsh::ExpressionType<>& expression_type,
			ElementOrigin                origin,
			Box<Expr>                    base,
			Box<Expr>                    index
		);
	};

	/**
	 * @brief Represents a call in an expression.
	 */
	struct CallExpr final: public Expr {
		base::Box<Expr>              callee;
		std::vector<base::Box<Expr>> arguments;

		CallExpr(
			query::Context&              ctx,
			ElementOrigin                origin,
			base::Box<Expr>              callee,
			std::vector<base::Box<Expr>> arguments
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		CallExpr(
			tsh::ExpressionType<>        expression_type,
			ElementOrigin                origin,
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

		SequenceExpr(
			query::Context& ctx, ElementOrigin origin, std::vector<base::Box<Expr>> expressions
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		SequenceExpr(
			tsh::ExpressionType<>        expression_type,
			ElementOrigin                origin,
			std::vector<base::Box<Expr>> expressions
		);
	};

	/**
	 * @brief Represents a chain comparison, like "a < b == c >= d"
	 * The result is a logical and of each separate comparison.
	 * @TODO: User defined comparison operators.
	 */
	struct ChainComparisonExpr final: public Expr {
		// A list of all comparison expressions.
		// E.g. in `a < b < c`, this will contain the expressions for `a < b` and `b < c`.
		// Note that `b` will typically be reused in both comparisons, via ReusableExpr.
		std::vector<Box<Expr>> comparisons;

		ChainComparisonExpr(
			query::Context& ctx, ElementOrigin origin, std::vector<Box<Expr>> comparisons
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		ChainComparisonExpr(
			tsh::ExpressionType<>  expression_type,
			ElementOrigin          origin,
			std::vector<Box<Expr>> comparisons
		);
	};

	/**
	 * @brief Represents a type cast expression for builtin types, such as "i64(..)".
	 *
	 * CastExpr performs a conversion of the source expression to the specified target type.
	 * The result is an expression of the target type.
	 */
	struct CastExpr final: public Expr {
		Box<Expr>         source_expr;
		tsh::SymbolType<> target_type;

		CastExpr(
			query::Context&   ctx,
			ElementOrigin     origin,
			Box<Expr>         source_expr,
			tsh::SymbolType<> target_type
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		CastExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
			Box<Expr>             source_expr,
			tsh::SymbolType<>     target_type
		);
	};

	/**
	 * @brief Represents a reference creation expression (refof).
	 * It takes an expression of type T and produces a value of type ref T.
	 */
	struct RefOfExpr final: public Expr {
		Box<Expr> inner;

		RefOfExpr(query::Context& ctx, ElementOrigin origin, Box<Expr> inner);
		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		RefOfExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> inner);
	};

	/**
	 * @brief Represents a pointer creation expression (`ptrof`).
	 * It takes a place holding a value of symbol type `S` and produces a value of type `ptr S`.
	 *
	 * Unlike `RefOfExpr`, the reference kind of the operand is kept instead of being collapsed into
	 * `Ref`: `ptrof` on a place of type `box T` yields `ptr box T`, the address of the box itself,
	 * and not a reference to its pointee. That makes it the way to address an element of a buffer
	 * whose element type is itself a reference or a box.
	 */
	struct PtrOfExpr final: public Expr {
		Box<Expr> inner;

		PtrOfExpr(query::Context& ctx, ElementOrigin origin, Box<Expr> inner);
		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		PtrOfExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> inner);
	};

	/**
	 * @brief Represents a move expression (`move x`, or an implicit move of a temporary).
	 *
	 * It takes a place of type T and produces a value of the same type, but as a temporary,
	 * signalling that ownership of the operand is transferred out of it. The source local is
	 * marked as moved during MIR lowering, so using it afterwards is a use-after-move.
	 */
	struct MoveExpr final: public Expr {
		/**
		 * @brief Why a `MoveExpr` was created.
		 *
		 * - `Explicit` comes from the `move` keyword written in the code.
		 * - `Implicit` is inserted by a coercion consuming an owned rvalue (a temporary) or when
		 * moving the return value out of the function.
		 *
		 * @note: This is only used for testing and easier debugging purposes. The semantics between
		 * the two don't differ.
		 */
		enum class MoveKind : std::uint8_t { Explicit, Implicit };

		Box<Expr> inner;
		MoveKind  kind;

		MoveExpr(
			query::Context& ctx,
			ElementOrigin   origin,
			Box<Expr>       inner,
			MoveKind        kind = MoveKind::Explicit
		);
		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		MoveExpr(
			tsh::ExpressionType<> expression_type,
			ElementOrigin         origin,
			Box<Expr>             inner,
			MoveKind              kind
		);
	};

	/**
	 * @brief Represents a dereference operation on a reference/box type.
	 *
	 * This node is inserted in three cases:
	 * - When a value of type `ref T` is coerced to `T`,
	 * - When `ref T`/`box T` appears on the left-hand side of the assignment operator (it's worth
	 * remembering that a reference is essentially a pointer with a convenient interface, thus all
	 * assignments to it need to perform a dereference).
	 * - During field access on a `ref T` / `box T` type.
	 *
	 * - In a context that requires a value, such as the right-hand side of an
	 * assignment (`let x: T = ref_val`), this expression resolves to the value
	 * pointed to by the reference and translates to a `load` instruction in LLVM.
	 *
	 * - In a context that requires a memory location, such as the left-hand side
	 * of an assignment (`ref_val = new_t;`), this expression resolves to the memory
	 * location itself, allowing it to be written to. This provides the address for a `store`
	 * instruction in LLVM.
	 */
	struct DerefExpr final: public Expr {
		Box<Expr> inner;

		DerefExpr(query::Context& ctx, ElementOrigin origin, Box<Expr> inner);
		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		DerefExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> inner);
	};

	/**
	 * @brief Represents a default (zeroed) value for a given type.
	 * Used for implicit variable initialization. This gets then mapped to `llvm::getNullValue(type)`.
	 */
	struct DefaultValueExpr final: public Expr {
		tsh::AbstractType type;

		DefaultValueExpr(query::Context& ctx, ElementOrigin origin, tsh::AbstractType type);
		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		DefaultValueExpr(
			tsh::ExpressionType<> expression_type, ElementOrigin origin, tsh::AbstractType type
		);
	};

	/**
	 * @brief Constructs an aggregate value element-by-element, in place.
	 *
	 * Covers both record-like aggregates (struct/class/tuple), where the elements are the fields in
	 * declaration order, and statically-sized arrays, where the elements are the array items in
	 * index order, or a single value that fills all of the elements of the array.
	 *
	 * It stores the into uninitialized storage, with only the last store flagged as constructing
	 * the destination, to avoid destructor insertion on assignment.
	 */
	struct CreateAggregateExpr final: public Expr {
		tsh::AbstractType            type;
		std::vector<base::Box<Expr>> values;

		/// This is only used when we fill the array elements with a loop,
		/// this statements will be lowered in a loop body.
		base::Optional<base::CSharedBox<CodeBlock>> per_element_body;

		CreateAggregateExpr(
			query::Context&                             ctx,
			ElementOrigin                               origin,
			tsh::AbstractType                           type,
			std::vector<base::Box<Expr>>                values,
			base::Optional<base::CSharedBox<CodeBlock>> per_element_body = std::nullopt
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		CreateAggregateExpr(
			tsh::ExpressionType<>                       expression_type,
			ElementOrigin                               origin,
			tsh::AbstractType                           type,
			std::vector<base::Box<Expr>>                values,
			base::Optional<base::CSharedBox<CodeBlock>> per_element_body
		);
	};

	/**
	 * @brief Represents a block of statements that evaluates to a single value.
	 */
	struct BlockExpr final: public Expr {
		// @TODO: #3292 Refactor once we figure out how a user should be able to use blocks in
		// expressions.
		Box<Stmt> block;

		BlockExpr(query::Context& ctx, ElementOrigin origin, Box<Stmt> block);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const final;

		[[nodiscard]] Box<Expr> clone() const final;

	private:
		FRIEND_MAKEBOX

		BlockExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Stmt> block);
	};
}

ID_STD_HASH(compiler::helios::code::HOUTExprID);
