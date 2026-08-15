/**
 * @file expr.cpp
 * @brief Implementation of methods in the Expr hierarchy.
 */

#include "expr.hpp"

#include "../visitors.hpp"
#include "stmt.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/context/context.hpp>

#include <utility>

namespace compiler::helios::code {

#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	EXPR_VISITOR(LiteralUnitExpr)
	EXPR_VISITOR(LiteralNumericExpr)
	EXPR_VISITOR(LiteralBoolExpr)
	EXPR_VISITOR(LiteralCharExpr)
	EXPR_VISITOR(LiteralStringExpr)
	EXPR_VISITOR(LiteralTypeExpr)
	EXPR_VISITOR(IdentifierExpr)
	EXPR_VISITOR(ReusableExpr)
	EXPR_VISITOR(BinaryOperatorExpr)
	EXPR_VISITOR(UnaryOperatorExpr)
	EXPR_VISITOR(TernaryOperatorExpr)
	EXPR_VISITOR(ChainComparisonExpr)
	EXPR_VISITOR(TupleExpr)
	EXPR_VISITOR(VariantTypeConstructorExpr)
	EXPR_VISITOR(CallExpr)
	EXPR_VISITOR(AccessExpr)
	EXPR_VISITOR(IndexExpr)
	EXPR_VISITOR(SequenceExpr)
	EXPR_VISITOR(MoveExpr)
	EXPR_VISITOR(RefOfExpr)
	EXPR_VISITOR(PtrOfExpr)
	EXPR_VISITOR(DerefExpr)
	EXPR_VISITOR(DefaultValueExpr)
	EXPR_VISITOR(CreateAggregateExpr)
	EXPR_VISITOR(CastExpr)
	EXPR_VISITOR(LiftToTypeExpr)
	EXPR_VISITOR(BlockExpr)
	EXPR_VISITOR(ListPushExpr)
	EXPR_VISITOR(ListPopExpr)

	LiteralUnitExpr::LiteralUnitExpr(query::Context&, ElementOrigin origin):
		  Expr(
			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  tsh::getUnitType(),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ) {}

	LiteralUnitExpr::LiteralUnitExpr(tsh::ExpressionType<> expression_type, ElementOrigin origin):
		  Expr(expression_type, origin) {}

	void LiteralUnitExpr::debugPrint(std::ostream& out) const { out << "()"; }

	Box<Expr> LiteralUnitExpr::clone() const {
		return makeBox<LiteralUnitExpr>(expression_type, origin);
	}

	LiteralNumericExpr::LiteralNumericExpr(
		query::Context& ctx, ElementOrigin origin, numeric_value::NumericValue value
	):
		  Expr(
			  tsh::ExpressionType<>(
				  value.getTypeOfStoredValue(ctx), tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ),
		  value(value) {}

	LiteralNumericExpr::LiteralNumericExpr(
		tsh::ExpressionType<>       expression_type,
		ElementOrigin               origin,
		numeric_value::NumericValue value
	):
		  Expr(expression_type, origin),
		  value(value) {}

	void LiteralNumericExpr::debugPrint(std::ostream& out) const { out << value.toString(); }

	Box<Expr> LiteralNumericExpr::clone() const {
		return makeBox<LiteralNumericExpr>(expression_type, origin, value);
	}

	LiteralBoolExpr::LiteralBoolExpr(query::Context&, ElementOrigin origin, bool value):
		  Expr(

			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  tsh::getBoolType(),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ),
		  value(value) {}

	LiteralBoolExpr::LiteralBoolExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, bool value
	):
		  Expr(expression_type, origin),
		  value(value) {}

	void LiteralBoolExpr::debugPrint(std::ostream& out) const { out << (value ? "true" : "false"); }

	Box<Expr> LiteralBoolExpr::clone() const {
		return makeBox<LiteralBoolExpr>(expression_type, origin, value);
	}

	LiteralCharExpr::LiteralCharExpr(query::Context&, const ElementOrigin& origin, char value):
		  Expr(
			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  tsh::getCharType(),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ),
		  value(value) {}

	void LiteralCharExpr::debugPrint(std::ostream& out) const { out << "'" << value << "'"; }

	LiteralCharExpr::LiteralCharExpr(
		const tsh::ExpressionType<>& expression_type, const ElementOrigin& origin, const char value
	):
		  Expr(expression_type, origin),
		  value(value) {}

	LiteralStringExpr::LiteralStringExpr(
		query::Context& ctx, ElementOrigin origin, const base::StrID value
	):
		  Expr(
			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  tsh::getCharSliceType(ctx),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ),
		  value(value) {}

	Box<Expr> LiteralCharExpr::clone() const {
		return makeBox<LiteralCharExpr>(expression_type, origin, value);
	}

	LiteralStringExpr::LiteralStringExpr(
		const tsh::ExpressionType<>& expression_type, ElementOrigin origin, const base::StrID value
	):
		  Expr(expression_type, origin),
		  value(value) {}

	void LiteralStringExpr::debugPrint(std::ostream& out) const { out << value.str(); }

	Box<Expr> LiteralStringExpr::clone() const {
		return makeBox<LiteralStringExpr>(expression_type, origin, value);
	}

	LiteralTypeExpr::LiteralTypeExpr(query::Context&, ElementOrigin origin, tsh::AbstractType type):
		  Expr(

			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  tsh::getMetaType(),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ),
		  value_type(tsh::SymbolType{
			  type,
			  tsh::ReferenceKind::Direct,
			  tsh::Mutability::Mutable,
		  }) {}

	LiteralTypeExpr::LiteralTypeExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, tsh::SymbolType<> value_type
	):
		  Expr(expression_type, origin),
		  value_type(value_type) {}

	void LiteralTypeExpr::debugPrint(std::ostream& out) const { out << value_type.toString(); }

	Box<Expr> LiteralTypeExpr::clone() const {
		return makeBox<LiteralTypeExpr>(expression_type, origin, value_type);
	}

	IdentifierExpr::IdentifierExpr(query::Context& ctx, ElementOrigin origin, SymID symbol):
		  Expr(

			  tsh::ExpressionType<>(
				  ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow(),
				  tsh::ValueCategory(tsh::primaryCategoryOfSymbol(ctx, symbol))
			  ),
			  origin
		  ),
		  symbol(symbol) {}

	IdentifierExpr::IdentifierExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, SymID symbol
	):
		  Expr(expression_type, origin),
		  symbol(symbol) {}

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << strConcat("(Symbol ", name(symbol), " (", symbol.queryUnstablePerfectHash(), "))");
	}

	Box<Expr> IdentifierExpr::clone() const {
		return makeBox<IdentifierExpr>(expression_type, origin, symbol);
	}

	/**
	 * @brief The `PrimaryCategory` of a reusable expression based on it's `inner` type.
	 *
	 * A reusable expression is evaluated once into a hidden local that every use reads afterwards,
	 * so a `Temporary` (an owned rvalue) becomes a `Local` (an owned lvalue). The hidden local is
	 * what owns the value now and what gets destroyed at the end of the scope.
	 *
	 * We can't just inherit the inner category of a `Temporary` here. As every read of the
	 * `ReusableExpr` would be an implicit move which would move the same value multiple times.
	 * Additionally, field reads off the reusable would be treated as partial moves, which are NYI.
	 *
	 * Every other category is already an lvalue or owns nothing (`Local`, `Global`, `Dereferenced`,
	 * `Literal`) so reading it repeatedly is safe (in case of trivially copyable types).
	 */
	static tsh::ExpressionType<> reusableExpressionType(const Expr& inner) {
		const tsh::ExpressionType<> inner_type = inner.expression_type;
		if (inner_type.getValueCategory().getCategory() != tsh::PrimaryCategory::Temporary)
			return inner_type;

		return { inner_type.getSymbolType(), tsh::ValueCategory(tsh::PrimaryCategory::Local) };
	}

	ReusableExpr::ReusableExpr(query::Context& ctx, Box<Expr> inner, const bool first_use):
		  Expr(reusableExpressionType(*inner), inner->origin),
		  inner(std::move(inner)),
		  first_use(first_use) {
		const tsh::ExpressionType<> inner_type = this->inner->expression_type;

		// MIR lowers a reusable expr by storing its inner value into a hidden local,
		// and that store is a plain byte-copy. This hidden local gets a destructor inserted
		// afterwards. The only cases where such byte-copy is legal (and won't cause a double free)
		// is:
		// - a trivially copyable value
		// - a `Temporary` which is an owned rvalue, so the hidden local takes its ownership over
		// and the temporary is destructed after use.
		//
		// Anything else is a place somebody already owns. Copying its bytes naively may cause a
		// double free. In this case, read the place again instead of reusing it, or bind a
		// reference to it and reuse that.
		//
		// @note: If you ever need to use reusable expr on an owned local, feel free to remove this
		// `CORE_ASSERT`, but figure out how to change `ReusableExpr` so duplicate destructors don't
		// get inserted.
		CORE_ASSERT(
			inner_type.getSymbolType().isTriviallyCopyable(ctx)
				or inner_type.getValueCategory().getCategory() == tsh::PrimaryCategory::Temporary,
			base::strConcat(
				"A ReusableExpr over `",
				inner_type.getSymbolType().toString(),
				"` would be stored by copying its bytes, but the value is not trivially "
				"copyable and not an owned rvalue, so the copy and the original would both be "
				"destroyed. Which will cause a double destructor call."
			)
		);
	}

	void ReusableExpr::debugPrint(std::ostream& out) const {
		if (first_use)
			out << "[tmp](";
		else
			out << "[reuse](";
		inner->debugPrint(out);
		out << ")";
	}

	Box<Expr> ReusableExpr::clone() const {
		// Using a concurrent static map isn't particularly elegant. Another solution would be to
		// add a "cloning context" (by default empty) to the clone methods and pass it around. It
		// could then also be a simple sequential data structure.
		// Consider adding a cloning context if we find more use cases for it, but for now
		// this is a simple solution for a local problem.
		static concurrent::ConHashMap<HOUTExprID, SharedBox<Expr>> inner_id_to_cloned;

		// We want to clone the inner expression only once, and reuse the cloned version for all
		// reusable expressions which pointed to the same inner expression. Hence, we keep a map
		// from the inner ID of the cloned expression to its clone
		// Note that when the reusable expressions get cloned *again*, the ID of the inner *clone*
		// is taken as the key to the map, and the clone of the clone is inserted.
		auto inner_cloned = [&] -> SharedBox<Expr> {
			auto inner_id = inner->getID();
			if (!inner_id_to_cloned.contains(inner_id)) {
				// Attempt to populate the map only if it doesn't already contain a cloned version
				// of the inner expression. This might fail if another thread populated the map after
				// the `contains` check above, in which case all is good, and we ignore the failure.
				inner_id_to_cloned.maybePut(inner_id, SharedBox(inner->clone()));
			}
			return *inner_id_to_cloned.at(inner_id);
		}();

		return makeBox<ReusableExpr>(inner_cloned, first_use);
	}

	Box<ReusableExpr> ReusableExpr::nextUse() const {
		return makeBox<ReusableExpr>(inner, /*first_use=*/false);
	}

	ReusableExpr::ReusableExpr(const SharedBox<Expr>& inner, const bool first_use):
		  Expr(reusableExpressionType(*inner), inner->origin),
		  inner(inner),
		  first_use(first_use) {}

	tsh::AbstractType builtinBinaryOperationToReturnType(
		query::Context&         ctx,
		const BuiltinBinary     operation,
		const tsh::AbstractType lhs_type,
		const tsh::AbstractType rhs_type
	) {
		using enum BuiltinBinary;
		switch (operation) {
		case IntegerAdd:
			if ((lhs_type.getKind() == tsh::Kind::Char && rhs_type.getKind() == tsh::Kind::Integral)
			    || (lhs_type.getKind() == tsh::Kind::Integral
			        && rhs_type.getKind() == tsh::Kind::Char))
				return tsh::getCharType();
			return lhs_type;
		case IntegerSub:
			if (lhs_type.getKind() == tsh::Kind::Char && rhs_type.getKind() == tsh::Kind::Char)
				return tsh::getIntegralType(ctx, 8, tsh::IntegralAbstractType::Signedness::Unsigned);
			return lhs_type;
		case IntegerMul:
		case IntegerDiv:
		case IntegerMod:
		case IntegerPow:
		case FloatAdd:
		case FloatSub:
		case FloatMul:
		case FloatDiv:
		case FloatMod:
		case FloatPow:
			return lhs_type;
		case IntegerLt:
		case IntegerGt:
		case IntegerLteq:
		case IntegerGteq:
		case IntegerEq:
		case IntegerNeq:
		case FloatLt:
		case FloatGt:
		case FloatLteq:
		case FloatGteq:
		case FloatEq:
		case FloatNeq:
		case MetaEq:
		case MetaNeq:
			return tsh::getBoolType();
		case BooleanAnd:
		case BooleanOr:
			return lhs_type;
		default:
			CORE_UNREACHABLE();
		}
	}

	BinaryOperatorExpr::BinaryOperatorExpr(
		query::Context& ctx,
		ElementOrigin   origin,
		BuiltinBinary   operation,
		Box<Expr>       lhs,
		Box<Expr>       rhs
	):
		  Expr(
			  tsh::ExpressionType<>(
				  // @TODO: #2171 Select type of expression based on result type of the operation.
				  tsh::SymbolType{
					  builtinBinaryOperationToReturnType(
						  ctx,
						  operation,
						  lhs->expression_type.getType(),
						  rhs->expression_type.getType()
					  ),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  operation(operation),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	BinaryOperatorExpr::BinaryOperatorExpr(
		tsh::ExpressionType<> expression_type,
		ElementOrigin         origin,
		BuiltinBinary         operation,
		base::Box<Expr>       lhs,
		base::Box<Expr>       rhs
	):
		  Expr(expression_type, origin),
		  operation(operation),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		// note: this might get more complex in the future

		lhs->debugPrint(out);
		out << " ";

		using enum BuiltinBinary;
		switch (operation) {
		case IntegerAdd:
		case FloatAdd:
			out << "+";
			break;
		case IntegerSub:
		case FloatSub:
			out << "-";
			break;
		case IntegerMul:
		case FloatMul:
			out << "*";
			break;
		case IntegerDiv:
		case FloatDiv:
			out << "/";
			break;
		case IntegerMod:
		case FloatMod:
			out << "%";
			break;
		case IntegerPow:
		case FloatPow:
			out << "**";
			break;
		case IntegerLt:
		case FloatLt:
			out << "<";
			break;
		case IntegerLteq:
		case FloatLteq:
			out << "<=";
			break;
		case IntegerGt:
		case FloatGt:
			out << ">";
			break;
		case IntegerGteq:
		case FloatGteq:
			out << ">=";
			break;
		case IntegerEq:
		case FloatEq:
		case MetaEq:
			out << "==";
			break;
		case IntegerNeq:
		case FloatNeq:
		case MetaNeq:
			out << "!=";
			break;
		case BooleanAnd:
			out << "and";
			break;
		case BooleanOr:
			out << "or";
			break;
		default:
			CORE_UNREACHABLE();
		}

		out << " ";
		rhs->debugPrint(out);
	}

	Box<Expr> BinaryOperatorExpr::clone() const {
		return makeBox<BinaryOperatorExpr>(
			expression_type, origin, operation, lhs->clone(), rhs->clone()
		);
	}

	TernaryOperatorExpr::TernaryOperatorExpr(
		query::Context&,
		ElementOrigin origin,
		Box<Expr>     condition,
		Box<Expr>     if_true,
		Box<Expr>     if_false
	):
		  Expr(if_true->expression_type, origin),
		  condition(std::move(condition)),
		  if_true(std::move(if_true)),
		  if_false(std::move(if_false)) {}

	TernaryOperatorExpr::TernaryOperatorExpr(
		tsh::ExpressionType<> expression_type,
		ElementOrigin         origin,
		Box<Expr>             condition,
		Box<Expr>             if_true,
		Box<Expr>             if_false
	):
		  Expr(expression_type, origin),
		  condition(std::move(condition)),
		  if_true(std::move(if_true)),
		  if_false(std::move(if_false)) {}

	void TernaryOperatorExpr::debugPrint(std::ostream& out) const {
		out << "if ";
		condition->debugPrint(out);
		out << " then ";
		if_true->debugPrint(out);
		out << " else ";
		if_false->debugPrint(out);
	}

	Box<Expr> TernaryOperatorExpr::clone() const {
		return makeBox<TernaryOperatorExpr>(
			expression_type, origin, condition->clone(), if_true->clone(), if_false->clone()
		);
	}

	/**
	 * Map a vector of HOUT expressions to their symbol types.
	 */
	std::vector<tsh::SymbolType<>> extractTypesFromExprs(const std::vector<Box<Expr>>& exprs) {
		std::vector<tsh::SymbolType<>> types;
		types.reserve(exprs.size());
		for (const auto& expr: exprs) types.push_back(expr->expression_type.getSymbolType());
		return types;
	}

	TupleExpr::TupleExpr(query::Context& ctx, ElementOrigin origin, std::vector<Box<Expr>> elements):
		  Expr(
			  tsh::ExpressionType{
				  tsh::SymbolType<>{
					  ctx.query<tsh::QueryTupleType>({ extractTypesFromExprs(elements) }),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary),
			  },
			  origin
		  ),
		  elements(std::move(elements)),
		  tuple_ctor_symbol(ctx.query<defgen::QueryGeneratedSymbol>(
			  { .name = ctx.query<mangler::QueryMangledType>(expression_type.getSymbolType())
	                        ->valueOrThrow(),
	            .generated_symbol_data
	            = defgen::Constructor{ .type = expression_type.getType(),
	                                   .kind = defgen::Constructor::Kind::Implicit } }
		  )) {}

	TupleExpr::TupleExpr(
		tsh::ExpressionType<>        expression_type,
		ElementOrigin                origin,
		std::vector<base::Box<Expr>> elements,
		SymID                        tuple_ctor_symbol
	):
		  Expr(expression_type, origin),
		  elements(std::move(elements)),
		  tuple_ctor_symbol(tuple_ctor_symbol) {}

	void TupleExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_comma = false; auto&& e: elements) {
			if (add_comma) out << ", ";
			e->debugPrint(out);
			add_comma = true;
		}
		out << ")";
	}

	Box<Expr> TupleExpr::clone() const {
		std::vector<base::Box<Expr>> elements;
		elements.reserve(this->elements.size());
		for (const auto& elem: this->elements) elements.push_back(elem->clone());
		return makeBox<TupleExpr>(expression_type, origin, std::move(elements), tuple_ctor_symbol);
	}

	VariantTypeConstructorExpr::VariantTypeConstructorExpr(
		query::Context&, ElementOrigin origin, std::vector<Box<Expr>> subtypes
	):
		  Expr(

			  tsh::ExpressionType{
				  tsh::SymbolType{
					  tsh::getMetaType(),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary),
			  },
			  origin
		  ),
		  subtypes(std::move(subtypes)) {}

	VariantTypeConstructorExpr::VariantTypeConstructorExpr(
		tsh::ExpressionType<>        expression_type,
		ElementOrigin                origin,
		std::vector<base::Box<Expr>> subtypes
	):
		  Expr(expression_type, origin),
		  subtypes(std::move(subtypes)) {}

	void VariantTypeConstructorExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_pipe = false; auto&& subtype: subtypes) {
			if (add_pipe) out << " | ";
			subtype->debugPrint(out);
			add_pipe = true;
		}
		out << ")";
	}

	Box<Expr> VariantTypeConstructorExpr::clone() const {
		std::vector<base::Box<Expr>> cloned_subtypes;
		cloned_subtypes.reserve(subtypes.size());
		for (const auto& subtype: subtypes) cloned_subtypes.push_back(subtype->clone());
		return makeBox<VariantTypeConstructorExpr>(
			expression_type, origin, std::move(cloned_subtypes)
		);
	}

	tsh::AbstractType builtinUnaryOperationToReturnType(
		[[maybe_unused]] query::Context& ctx, BuiltinUnary operation, tsh::AbstractType argument_type
	) {
		using enum BuiltinUnary;
		switch (operation) {
		case BuiltinUnary::IntegerNegation:
		case BuiltinUnary::FloatNegation:
		case BuiltinUnary::BooleanNot:
		case BuiltinUnary::Ref:
		case BuiltinUnary::Box:
		case BuiltinUnary::Ptr:
		case BuiltinUnary::CPtr:
		case BuiltinUnary::ManyPtr:
		case BuiltinUnary::Slice:
		case BuiltinUnary::Const:
			// For most of the unary operators the result is the same as their argument type:
			// (Int -> Int, Bool -> Bool, Meta -> Meta, etc.)
			return argument_type;
		case BuiltinUnary::SizeOf:
		case BuiltinUnary::AlignOf:
			// `size_of`/`align_of` map a meta type to an `i64` byte count.
			return tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
		default:
			CORE_UNREACHABLE();
		}
	}

	UnaryOperatorExpr::UnaryOperatorExpr(
		query::Context& ctx, ElementOrigin origin, BuiltinUnary operation, Box<Expr> expr
	):
		  Expr(
			  tsh::ExpressionType<>{
				  tsh::SymbolType<>{ builtinUnaryOperationToReturnType(
										 ctx, operation, expr->expression_type.getType()
									 ),
	                                 tsh::ReferenceKind::Direct,
	                                 tsh::Mutability::Mutable },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary) },
			  origin
		  ),
		  operation(operation),
		  expr(std::move(expr)) {}

	UnaryOperatorExpr::UnaryOperatorExpr(
		tsh::ExpressionType<> expression_type,
		ElementOrigin         origin,
		BuiltinUnary          operation,
		base::Box<Expr>       expr
	):
		  Expr(expression_type, origin),
		  operation(operation),
		  expr(std::move(expr)) {}

	void UnaryOperatorExpr::debugPrint(std::ostream& out) const {
		switch (operation) {
		case BuiltinUnary::IntegerNegation:
		case BuiltinUnary::FloatNegation:
			out << "-";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::BooleanNot:
			out << "not ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::Ref:
			out << "ref ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::Box:
			out << "box ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::Const:
			out << "const ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::Ptr:
			out << "ptr ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::CPtr:
			out << "cptr ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::ManyPtr:
			out << "manyptr ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::Slice:
			out << "slice ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::SizeOf:
			out << "size_of ";
			expr->debugPrint(out);
			break;
		case BuiltinUnary::AlignOf:
			out << "align_of ";
			expr->debugPrint(out);
			break;
		default:
			CORE_PANIC("unsupported unary operation");
		}
	}

	Box<Expr> UnaryOperatorExpr::clone() const {
		return makeBox<UnaryOperatorExpr>(expression_type, origin, operation, expr->clone());
	}

	namespace {
		/**
		 * @brief Infers a resulting type from a call operation.
		 * @note This will be here until we have a proper overload resolution.
		 */
		tsh::SymbolType<> getCallResultType(tsh::AbstractType tp) {
			if (tp.getKind() == tsh::Kind::Function) {
				auto func = tsh::FunctionAbstractType(tp);
				return func.getResultType();
			}
			CORE_PANIC("Invalid kind to call: ", base::enumToStr(tp.getKind()));
		}
	}

	CallExpr::CallExpr(
		query::Context&,
		ElementOrigin          origin,
		base::Box<Expr>        callee,
		std::vector<Box<Expr>> arguments
	):
		  Expr(
			  tsh::ExpressionType(
				  getCallResultType(callee->expression_type.getType()),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  callee(std::move(callee)),
		  arguments(std::move(arguments)) {}

	CallExpr::CallExpr(
		tsh::ExpressionType<>        expression_type,
		ElementOrigin                origin,
		base::Box<Expr>              callee,
		std::vector<base::Box<Expr>> arguments
	):
		  Expr(expression_type, origin),
		  callee(std::move(callee)),
		  arguments(std::move(arguments)) {}

	void CallExpr::debugPrint(std::ostream& out) const {
		callee->debugPrint(out);
		out << "(";
		bool add_comma = false;
		for (auto&& arg: arguments) {
			if (add_comma) out << ", ";
			arg->debugPrint(out);
			add_comma = true;
		}
		out << ")";
	}

	Box<Expr> CallExpr::clone() const {
		std::vector<base::Box<Expr>> arguments_cloned;
		arguments_cloned.reserve(arguments.size());
		for (const auto& arg: arguments) arguments_cloned.push_back(arg->clone());
		return makeBox<CallExpr>(
			expression_type, origin, callee->clone(), std::move(arguments_cloned)
		);
	}

	AccessExpr::AccessExpr(
		query::Context& ctx, ElementOrigin origin, Box<Expr> base, const SymID field
	):
		  Expr(
			  tsh::ExpressionType(
				  ctx.query<QueryTypeOfSymbol>(field)->valueOrThrow(),
				  tsh::ValueCategory(base->expression_type.getValueCategory().withDisabled(
					  tsh::ValueSemanticsOptions::MOVE
				  ))
			  ),
			  origin
		  ),
		  base(std::move(base)),
		  field(field) {
		CORE_ASSERT(kind(field) == SymbolKind::Field, "Field in AccessExpr must be a field symbol");
	}

	AccessExpr::AccessExpr(
		const tsh::ExpressionType<>& expression_type,
		ElementOrigin                origin,
		Box<Expr>                    base,
		const SymID                  field
	):
		  Expr(expression_type, origin),
		  base(std::move(base)),
		  field(field) {
		CORE_ASSERT(kind(field) == SymbolKind::Field, "Field in AccessExpr must be a field symbol");
	}

	void AccessExpr::debugPrint(std::ostream& out) const {
		base->debugPrint(out);
		out << "." << name(field).str();
	}

	Box<Expr> AccessExpr::clone() const {
		return makeBox<AccessExpr>(expression_type, origin, base->clone(), field);
	}

	IndexExpr::IndexExpr(query::Context&, ElementOrigin origin, Box<Expr> base, Box<Expr> index):
		  Expr(
			  tsh::ExpressionType(
				  [&]() -> tsh::SymbolType<> {
					  auto base_type = base->expression_type.getType();
					  switch (base_type.getKind()) {
					  case tsh::Kind::Meta: {  // Array type creation. The result of the index
			                                   // expression on meta is meta as well.
						  return base->expression_type.getSymbolType();
					  }
					  case tsh::Kind::DynamicArray:
						  return base_type.as<tsh::DynamicArrayAbstractType>().getElementType();
					  case tsh::Kind::StaticArray:
						  return base_type.as<tsh::StaticArrayAbstractType>().getElementType();
					  case tsh::Kind::ManyPointer:
						  return base_type.as<tsh::ManyPointerAbstractType>().getPointee();
					  case tsh::Kind::Slice:
						  return base_type.as<tsh::SliceAbstractType>().getElementType();
					  default:
						  CORE_PANIC("Cannot index a non-array like type");
					  }
				  }(),
				  tsh::ValueCategory(base->expression_type.getValueCategory().withDisabled(
					  tsh::ValueSemanticsOptions::MOVE
				  ))
			  ),
			  origin


		  ),
		  base(std::move(base)),
		  index(std::move(index)) {}

	IndexExpr::IndexExpr(
		const tsh::ExpressionType<>& expression_type,
		ElementOrigin                origin,
		Box<Expr>                    base,
		Box<Expr>                    index
	):
		  Expr(expression_type, origin),
		  base(std::move(base)),
		  index(std::move(index)) {}

	void IndexExpr::debugPrint(std::ostream& out) const {
		base->debugPrint(out);
		out << "[";
		index->debugPrint(out);
		out << "]";
	}

	Box<Expr> IndexExpr::clone() const {
		return makeBox<IndexExpr>(expression_type, origin, base->clone(), index->clone());
	}

	SequenceExpr::SequenceExpr(
		query::Context&, ElementOrigin origin, std::vector<Box<Expr>> expressions
	):
		  Expr(expressions.back()->expression_type, origin),
		  expressions(std::move(expressions)) {}

	SequenceExpr::SequenceExpr(
		tsh::ExpressionType<>        expression_type,
		ElementOrigin                origin,
		std::vector<base::Box<Expr>> expressions
	):
		  Expr(expression_type, origin),
		  expressions(std::move(expressions)) {}

	void SequenceExpr::debugPrint(std::ostream& out) const {
		for (bool add_comma = false; auto&& expr: expressions) {
			if (add_comma) out << ", ";
			expr->debugPrint(out);
			add_comma = true;
		}
	}

	Box<Expr> SequenceExpr::clone() const {
		std::vector<base::Box<Expr>> expressions;
		expressions.reserve(this->expressions.size());
		for (const auto& expr: this->expressions) expressions.push_back(expr->clone());
		return makeBox<SequenceExpr>(expression_type, origin, std::move(expressions));
	}

	ChainComparisonExpr::ChainComparisonExpr(
		query::Context&, ElementOrigin origin, std::vector<Box<Expr>> comparisons
	):
		  Expr(
			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  tsh::getBoolType(),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  comparisons{ std::move(comparisons) } {
		CORE_ASSERT(
			this->comparisons.size() > 0, "ChainComparisonExpr must have at least one comparison"
		);
	}

	ChainComparisonExpr::ChainComparisonExpr(
		tsh::ExpressionType<>  expression_type,
		ElementOrigin          origin,
		std::vector<Box<Expr>> comparisons
	):
		  Expr(expression_type, origin),
		  comparisons{ std::move(comparisons) } {}

	void ChainComparisonExpr::debugPrint(std::ostream& out) const {
		using namespace std::views;

		std::string separator = "";
		for (auto& comp: comparisons) {
			out << separator;
			comp->debugPrint(out);
			separator = " and ";
		}
	}

	Box<Expr> ChainComparisonExpr::clone() const {
		std::vector<Box<Expr>> comparisons;
		comparisons.reserve(this->comparisons.size());
		for (const auto& comp: this->comparisons) comparisons.push_back(comp->clone());
		return makeBox<ChainComparisonExpr>(expression_type, origin, std::move(comparisons));
	}

	CastExpr::CastExpr(
		query::Context&, ElementOrigin origin, Box<Expr> source_expr, tsh::SymbolType<> target_type
	):
		  Expr(
			  tsh::ExpressionType<>(
				  target_type, tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  source_expr(std::move(source_expr)),
		  target_type(target_type) {}

	CastExpr::CastExpr(
		tsh::ExpressionType<> expression_type,
		ElementOrigin         origin,
		Box<Expr>             source_expr,
		tsh::SymbolType<>     target_type
	):
		  Expr(expression_type, origin),
		  source_expr(std::move(source_expr)),
		  target_type(target_type) {}

	void CastExpr::debugPrint(std::ostream& out) const {
		out << "cast[to=" << target_type.toString() << "](";
		source_expr->debugPrint(out);
		out << ")";
	}

	Box<Expr> CastExpr::clone() const {
		return makeBox<CastExpr>(expression_type, origin, source_expr->clone(), target_type);
	}

	RefOfExpr::RefOfExpr(query::Context&, ElementOrigin origin, Box<Expr> inner):
		  Expr(
			  tsh::ExpressionType<>(
				  inner->expression_type.getSymbolType().withReferenceKind(tsh::ReferenceKind::Ref),
				  tsh::ValueCategory(
					  [](const tsh::ExpressionType<>& inner_type) -> tsh::PrimaryCategory {
						  switch (inner_type.getSymbolType().getRefKind()) {
						  case tsh::ReferenceKind::Direct:
							  return tsh::PrimaryCategory::Temporary;
						  case tsh::ReferenceKind::Ref:
							  return inner_type.getValueCategory().getCategory();
						  case tsh::ReferenceKind::Box:
							  return tsh::PrimaryCategory::Dereferenced;
						  }
						  CORE_UNREACHABLE();
					  }(inner->expression_type)
				  )
			  ),
			  origin
		  ),
		  inner(std::move(inner)) {}

	RefOfExpr::RefOfExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> inner
	):
		  Expr(expression_type, origin),
		  inner(std::move(inner)) {}

	void RefOfExpr::debugPrint(std::ostream& out) const {
		out << "refof(";
		inner->debugPrint(out);
		out << ")";
	}

	Box<Expr> RefOfExpr::clone() const {
		return makeBox<RefOfExpr>(expression_type, origin, inner->clone());
	}

	PtrOfExpr::PtrOfExpr(query::Context& ctx, ElementOrigin origin, Box<Expr> inner):
		  Expr(
			  tsh::ExpressionType<>(
				  // The whole symbol type of the operand becomes the pointee, so its reference kind
	              // survives: `ptrof` on a `box T` place gives `ptr box T`.
				  tsh::SymbolType<>::withDefaults(
					  ctx.query<tsh::QueryPointerType>({ inner->expression_type.getSymbolType() })
				  ),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  inner(std::move(inner)) {}

	PtrOfExpr::PtrOfExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> inner
	):
		  Expr(expression_type, origin),
		  inner(std::move(inner)) {}

	void PtrOfExpr::debugPrint(std::ostream& out) const {
		out << "ptrof(";
		inner->debugPrint(out);
		out << ")";
	}

	Box<Expr> PtrOfExpr::clone() const {
		return makeBox<PtrOfExpr>(expression_type, origin, inner->clone());
	}

	MoveExpr::MoveExpr(query::Context&, ElementOrigin origin, Box<Expr> inner, const MoveKind kind):
		  Expr(
			  tsh::ExpressionType<>(
				  inner->expression_type.getSymbolType(),
				  // `move` forces the move semantic.
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
					  .withForceSemantics(tsh::ValueSemanticsOptions::MOVE)
			  ),
			  origin
		  ),
		  inner(std::move(inner)),
		  kind(kind) {}

	MoveExpr::MoveExpr(
		tsh::ExpressionType<> expression_type,
		ElementOrigin         origin,
		Box<Expr>             inner,
		const MoveKind        kind
	):
		  Expr(expression_type, origin),
		  inner(std::move(inner)),
		  kind(kind) {}

	void MoveExpr::debugPrint(std::ostream& out) const {
		out << (kind == MoveKind::Implicit ? "implicit_move(" : "move(");
		inner->debugPrint(out);
		out << ")";
	}

	Box<Expr> MoveExpr::clone() const {
		return makeBox<MoveExpr>(expression_type, origin, inner->clone(), kind);
	}

	DerefExpr::DerefExpr(query::Context&, ElementOrigin origin, Box<Expr> inner):
		  Expr(
			  tsh::ExpressionType<>(
				  inner->expression_type.getSymbolType().getPointeeSymbolType(),
				  // Dereferencing creates a non-owned lvalue.
				  tsh::ValueCategory(tsh::PrimaryCategory::Dereferenced)
			  ),
			  origin
		  ),
		  inner(std::move(inner)) {}

	DerefExpr::DerefExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> inner
	):
		  Expr(expression_type, origin),
		  inner(std::move(inner)) {}

	void DerefExpr::debugPrint(std::ostream& out) const {
		out << "deref(";
		inner->debugPrint(out);
		out << ")";
	}

	Box<Expr> DerefExpr::clone() const {
		return makeBox<DerefExpr>(expression_type, origin, inner->clone());
	}

	DefaultValueExpr::DefaultValueExpr(query::Context&, ElementOrigin origin, tsh::AbstractType type):
		  Expr(
			  tsh::ExpressionType<>(
				  tsh::SymbolType<>{
					  type,
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  ),
			  origin
		  ),
		  type(type) {}

	DefaultValueExpr::DefaultValueExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, tsh::AbstractType type
	):
		  Expr(expression_type, origin),
		  type(type) {}

	void DefaultValueExpr::debugPrint(std::ostream& out) const {
		out << "default_value(" << expression_type.getSymbolType().toString() << ")";
	}

	Box<Expr> DefaultValueExpr::clone() const {
		return makeBox<DefaultValueExpr>(expression_type, origin, type);
	}

	CreateAggregateExpr::CreateAggregateExpr(
		query::Context&,
		ElementOrigin                               origin,
		tsh::AbstractType                           type,
		std::vector<Box<Expr>>                      values,
		base::Optional<base::CSharedBox<CodeBlock>> per_element_body
	):
		  Expr(
			  tsh::ExpressionType<>(
				  tsh::SymbolType<>{
					  type,
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  type(type),
		  values(std::move(values)),
		  per_element_body(std::move(per_element_body)) {}

	CreateAggregateExpr::CreateAggregateExpr(
		tsh::ExpressionType<>                       expression_type,
		ElementOrigin                               origin,
		tsh::AbstractType                           type,
		std::vector<Box<Expr>>                      values,
		base::Optional<base::CSharedBox<CodeBlock>> per_element_body
	):
		  Expr(expression_type, origin),
		  type(type),
		  values(std::move(values)),
		  per_element_body(std::move(per_element_body)) {}

	void CreateAggregateExpr::debugPrint(std::ostream& out) const {
		out << "create_aggregate(" << expression_type.getSymbolType().toString() << ") {";
		for (bool add_comma = false; auto&& v: values) {
			if (add_comma) out << ", ";
			v->debugPrint(out);
			add_comma = true;
		}
		out << "}";

		if (per_element_body.empty()) return;

		out << " per_element {\n";
		for (const auto& stmt: (*per_element_body)->statements) stmt->debugPrint(out, 1);
		out << "}";
	}

	Box<Expr> CreateAggregateExpr::clone() const {
		std::vector<base::Box<Expr>> cloned;
		cloned.reserve(values.size());
		for (const auto& v: values) cloned.push_back(v->clone());
		return makeBox<CreateAggregateExpr>(
			expression_type, origin, type, std::move(cloned), per_element_body
		);
	}

	LiftToTypeExpr::LiftToTypeExpr(query::Context&, ElementOrigin origin, Box<Expr> value_expr):
		  Expr(
			  tsh::ExpressionType(
				  tsh::SymbolType<>(
					  tsh::getMetaType(), tsh::ReferenceKind::Direct, tsh::Mutability::Mutable
				  ),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  value_expr(std::move(value_expr)) {}

	LiftToTypeExpr::LiftToTypeExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> value_expr
	):
		  Expr(expression_type, origin),
		  value_expr(std::move(value_expr)) {}

	void LiftToTypeExpr::debugPrint(std::ostream& out) const {
		out << "lift[to=type](";
		value_expr->debugPrint(out);
		out << ")";
	}

	Box<Expr> LiftToTypeExpr::clone() const {
		return makeBox<LiftToTypeExpr>(expression_type, origin, value_expr->clone());
	}

	BlockExpr::BlockExpr(query::Context&, ElementOrigin origin, Box<Stmt> block):
		  Expr(
			  tsh::ExpressionType(
				  tsh::SymbolType<>::withDefaults(tsh::getUnitType()),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  block(std::move(block)) {}

	BlockExpr::BlockExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Stmt> block
	):
		  Expr(expression_type, origin),
		  block(std::move(block)) {}

	void BlockExpr::debugPrint(std::ostream& out) const {
		out << "block(";
		block->debugPrint(out);
		out << ")";
	}

	Box<Expr> BlockExpr::clone() const {
		return makeBox<BlockExpr>(expression_type, origin, block->clone());
	}

	ListPushExpr::ListPushExpr(ElementOrigin origin, Box<Expr> list, Box<Expr> element):
		  Expr(
			  tsh::ExpressionType(
				  tsh::SymbolType<>(
					  tsh::getUnitType(), tsh::ReferenceKind::Direct, tsh::Mutability::Mutable
				  ),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  list(std::move(list)),
		  element(std::move(element)) {}

	ListPushExpr::ListPushExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> list, Box<Expr> element
	):
		  Expr(expression_type, origin),
		  list(std::move(list)),
		  element(std::move(element)) {}

	void ListPushExpr::debugPrint(std::ostream& out) const {
		out << "list_push(";
		list->debugPrint(out);
		out << ", ";
		element->debugPrint(out);
		out << ")";
	}

	Box<Expr> ListPushExpr::clone() const {
		return makeBox<ListPushExpr>(expression_type, origin, list->clone(), element->clone());
	}

	ListPopExpr::ListPopExpr(ElementOrigin origin, Box<Expr> list, Box<Expr> count):
		  Expr(
			  tsh::ExpressionType(
				  tsh::SymbolType<>(
					  tsh::getUnitType(), tsh::ReferenceKind::Direct, tsh::Mutability::Mutable
				  ),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  ),
			  origin
		  ),
		  list(std::move(list)),
		  count(std::move(count)) {}

	ListPopExpr::ListPopExpr(
		tsh::ExpressionType<> expression_type, ElementOrigin origin, Box<Expr> list, Box<Expr> count
	):
		  Expr(expression_type, origin),
		  list(std::move(list)),
		  count(std::move(count)) {}

	void ListPopExpr::debugPrint(std::ostream& out) const {
		out << "list_pop(";
		list->debugPrint(out);
		out << ", ";
		count->debugPrint(out);
		out << ")";
	}

	Box<Expr> ListPopExpr::clone() const {
		return makeBox<ListPopExpr>(expression_type, origin, list->clone(), count->clone());
	}

}
