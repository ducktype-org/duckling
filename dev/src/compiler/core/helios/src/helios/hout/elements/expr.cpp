/**
 * @file expr.cpp
 * @brief Implementation of methods in the Expr hierarchy.
 */

#include "expr.hpp"

#include "../visitors.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries.hpp>

#include <query_framework/context.hpp>

namespace compiler::helios::code {

#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	EXPR_VISITOR(LiteralUnitExpr)
	EXPR_VISITOR(LiteralIntExpr)
	EXPR_VISITOR(LiteralBoolExpr)
	EXPR_VISITOR(LiteralStringExpr)
	EXPR_VISITOR(LiteralTypeExpr)
	EXPR_VISITOR(IdentifierExpr)
	EXPR_VISITOR(BinaryOperatorExpr)
	EXPR_VISITOR(UnaryOperatorExpr)
	EXPR_VISITOR(TernaryOperatorExpr)
	EXPR_VISITOR(ChainComparisonExpr)
	EXPR_VISITOR(TupleTypeConstructorExpr)
	EXPR_VISITOR(VariantTypeConstructorExpr)
	EXPR_VISITOR(ParenthesisExpr)
	EXPR_VISITOR(CallExpr)
	EXPR_VISITOR(AccessExpr)
	EXPR_VISITOR(SequenceExpr)

	LiteralUnitExpr::LiteralUnitExpr(query::Context& ctx):
		  Expr(tsh::ExpressionType<>(
			  tsh::SymbolType{
				  // The unit expression *may* represent the type instead of the unit value,
				  // but by default we assume it is the value, and lazily convert it to a type,
				  // when it turns out that we expected a type instead of a value.
				  // TODO: (this PR), determine whether we need to distinguish between the two.
				  ctx.query<tsh::QueryUnitType>({}),
				  tsh::ReferenceKind::Direct,
				  tsh::Mutability::Immutable,
			  },
			  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
		  )) {}

	void LiteralUnitExpr::debugPrint(std::ostream& out) const { out << "()"; }

	LiteralIntExpr::LiteralIntExpr(query::Context& ctx, i64 value):
		  Expr(

			  tsh::ExpressionType<>(
				  // @TODO: Select type of expression based on type of literal.
				  tsh::SymbolType{
					  ctx.query<tsh::QueryIntegralType>({ 64 }),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	LiteralIntExpr::LiteralIntExpr(tsh::ExpressionType<> expression_type, i64 value):
		  Expr(expression_type),
		  value(value) {}

	void LiteralIntExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

	Box<Expr> LiteralIntExpr::clone() const {
		return makeBox<LiteralIntExpr>(expression_type, value);
	}

	LiteralBoolExpr::LiteralBoolExpr(query::Context& ctx, bool value):
		  Expr(

			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  ctx.query<tsh::QueryBoolType>({}),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	LiteralBoolExpr::LiteralBoolExpr(tsh::ExpressionType<> expression_type, bool value):
		  Expr(expression_type),
		  value(value) {}

	void LiteralBoolExpr::debugPrint(std::ostream& out) const { out << (value ? "true" : "false"); }

	Box<Expr> LiteralBoolExpr::clone() const {
		return makeBox<LiteralBoolExpr>(expression_type, value);
	}

	LiteralStringExpr::LiteralStringExpr(query::Context& ctx, tpc::StringValue value):
		  Expr(

			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  ctx.query<tsh::QueryStringType>({}),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	LiteralStringExpr::LiteralStringExpr(
		tsh::ExpressionType<> expression_type, tpc::StringValue value
	):
		  Expr(expression_type),
		  value(value) {}

	void LiteralStringExpr::debugPrint(std::ostream& out) const { out << value.str(); }

	Box<Expr> LiteralStringExpr::clone() const {
		return makeBox<LiteralStringExpr>(expression_type, value);
	}

	LiteralTypeExpr::LiteralTypeExpr(query::Context& ctx, tsh::AbstractType type):
		  Expr(

			  tsh::ExpressionType<>(
				  tsh::SymbolType{
					  ctx.query<tsh::QueryMetaType>({}),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value_type(tsh::SymbolType{
			  type,
			  tsh::ReferenceKind::Direct,
			  tsh::Mutability::Mutable,
		  }) {}

	LiteralTypeExpr::LiteralTypeExpr(
		tsh::ExpressionType<> expression_type, tsh::SymbolType<> value_type
	):
		  Expr(expression_type),
		  value_type(value_type) {}

	void LiteralTypeExpr::debugPrint(std::ostream& out) const { out << value_type.toString(); }

	Box<Expr> LiteralTypeExpr::clone() const {
		return makeBox<LiteralTypeExpr>(expression_type, value_type);
	}

	IdentifierExpr::IdentifierExpr(query::Context& ctx, SymID symbol):
		  Expr(

			  tsh::ExpressionType<>(
				  ctx.query<QueryTypeOfSymbol>(symbol)->expect(
					  "Handling errors in HOUT is not supported yet"
				  ),
				  tsh::ValueCategory(tsh::primaryCategoryOfSymbol(symbol))
			  )
		  ),
		  symbol(symbol) {}

	IdentifierExpr::IdentifierExpr(tsh::ExpressionType<> expression_type, SymID symbol):
		  Expr(expression_type),
		  symbol(symbol) {}

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << strConcat("(Symbol ", name(symbol), " (", symbol.queryUnstablePerfectHash(), "))");
	}

	Box<Expr> IdentifierExpr::clone() const {
		return makeBox<IdentifierExpr>(expression_type, symbol);
	}

	tsh::AbstractType builtinOperationToReturnType(
		query::Context& ctx, BuiltinBinary operation, tsh::AbstractType argument_type
	) {
		using enum BuiltinBinary;
		switch (operation) {
		case IntegerAdd:
		case IntegerSub:
		case IntegerMul:
		case IntegerDiv:
		case IntegerMod:
		case IntegerPow:
			return argument_type;
		case IntegerLt:
		case IntegerGt:
		case IntegerLteq:
		case IntegerGteq:
		case IntegerEq:
		case IntegerNeq:
			return ctx.query<tsh::QueryBoolType>({});
		case BooleanAnd:
		case BooleanOr:
			return argument_type;
		default:
			CORE_UNREACHABLE();
		}
	}

	BinaryOperatorExpr::BinaryOperatorExpr(
		query::Context& ctx, BuiltinBinary operation, Box<Expr> lhs, Box<Expr> rhs
	):
		  Expr(

			  tsh::ExpressionType<>(
				  // @TODO: Select type of expression based on result type of the operation.
				  tsh::SymbolType{
					  builtinOperationToReturnType(ctx, operation, lhs->expression_type.getType()),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Mutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  )
		  ),
		  operation(operation),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	BinaryOperatorExpr::BinaryOperatorExpr(
		tsh::ExpressionType<> expression_type,
		BuiltinBinary         operation,
		base::Box<Expr>       lhs,
		base::Box<Expr>       rhs
	):
		  Expr(expression_type),
		  operation(operation),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		// note: this might get more complex in the future

		lhs->debugPrint(out);

		using enum BuiltinBinary;
		switch (operation) {
		case IntegerAdd:
			out << "+";
			break;
		case IntegerSub:
			out << "-";
			break;
		case IntegerMul:
			out << "*";
			break;
		case IntegerDiv:
			out << "/";
			break;
		case IntegerMod:
			out << "%";
			break;
		case IntegerPow:
			out << "**";
			break;
		case BooleanAnd:
			out << " and ";
			break;
		case BooleanOr:
			out << " or ";
			break;
		default:
			CORE_UNREACHABLE();
		}
		rhs->debugPrint(out);
	}

	Box<Expr> BinaryOperatorExpr::clone() const {
		return makeBox<BinaryOperatorExpr>(expression_type, operation, lhs->clone(), rhs->clone());
	}

	ParenthesisExpr::ParenthesisExpr(query::Context&, Box<Expr> inner):
		  Expr(inner->expression_type),
		  inner(std::move(inner)) {}

	ParenthesisExpr::ParenthesisExpr(tsh::ExpressionType<> expression_type, base::Box<Expr> inner):
		  Expr(expression_type),
		  inner(std::move(inner)) {}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	Box<Expr> ParenthesisExpr::clone() const {
		return makeBox<ParenthesisExpr>(expression_type, inner->clone());
	}

	TernaryOperatorExpr::TernaryOperatorExpr(
		query::Context&, Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false
	):
		  Expr(if_true->expression_type),
		  condition(std::move(condition)),
		  if_true(std::move(if_true)),
		  if_false(std::move(if_false)) {}

	TernaryOperatorExpr::TernaryOperatorExpr(
		tsh::ExpressionType<> expression_type,
		Box<Expr>             condition,
		Box<Expr>             if_true,
		Box<Expr>             if_false
	):
		  Expr(expression_type),
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
			expression_type, condition->clone(), if_true->clone(), if_false->clone()
		);
	}

	TupleTypeConstructorExpr::TupleTypeConstructorExpr(
		query::Context& ctx, std::vector<Box<Expr>> elements
	):
		  Expr(

			  tsh::ExpressionType{
				  tsh::SymbolType{
					  ctx.query<tsh::QueryMetaType>({}),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary),
			  }
		  ),
		  elements(std::move(elements)) {}

	TupleTypeConstructorExpr::TupleTypeConstructorExpr(
		tsh::ExpressionType<> expression_type, std::vector<base::Box<Expr>> elements
	):
		  Expr(expression_type),
		  elements(std::move(elements)) {}

	void TupleTypeConstructorExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_comma = false; auto&& e: elements) {
			if (add_comma) out << ", ";
			e->debugPrint(out);
			add_comma = true;
		}
		out << ")";
	}

	Box<Expr> TupleTypeConstructorExpr::clone() const {
		std::vector<base::Box<Expr>> elements;
		elements.reserve(this->elements.size());
		for (const auto& elem: this->elements) elements.push_back(elem->clone());
		return makeBox<TupleTypeConstructorExpr>(expression_type, std::move(elements));
	}

	VariantTypeConstructorExpr::VariantTypeConstructorExpr(
		query::Context& ctx, std::vector<Box<Expr>> subtypes
	):
		  Expr(

			  tsh::ExpressionType{
				  tsh::SymbolType{
					  ctx.query<tsh::QueryMetaType>({}),
					  tsh::ReferenceKind::Direct,
					  tsh::Mutability::Immutable,
				  },
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary),
			  }
		  ),
		  subtypes(std::move(subtypes)) {}

	VariantTypeConstructorExpr::VariantTypeConstructorExpr(
		tsh::ExpressionType<> expression_type, std::vector<base::Box<Expr>> subtypes
	):
		  Expr(expression_type),
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
		return makeBox<VariantTypeConstructorExpr>(expression_type, std::move(cloned_subtypes));
	}

	UnaryOperatorExpr::UnaryOperatorExpr(BuiltinUnary operation, Box<Expr> expr):
		  Expr(expr->expression_type),
		  operation(operation),
		  expr(std::move(expr)) {}

	UnaryOperatorExpr::UnaryOperatorExpr(
		tsh::ExpressionType<> expression_type, BuiltinUnary operation, base::Box<Expr> expr
	):
		  Expr(expression_type),
		  operation(operation),
		  expr(std::move(expr)) {}

	void UnaryOperatorExpr::debugPrint(std::ostream& out) const {
		switch (operation) {
		case BuiltinUnary::IntegerNegation:
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
		default:
			CORE_PANIC("unsupported unary operation");
		}
	}

	Box<Expr> UnaryOperatorExpr::clone() const {
		return makeBox<UnaryOperatorExpr>(expression_type, operation, expr->clone());
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

	CallExpr::CallExpr(query::Context&, base::Box<Expr> callee, std::vector<Box<Expr>> arguments):
		  Expr(tsh::ExpressionType(
			  getCallResultType(callee->expression_type.getType()),
			  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
		  )),
		  callee(std::move(callee)),
		  arguments(std::move(arguments)) {}

	CallExpr::CallExpr(
		tsh::ExpressionType<>        expression_type,
		base::Box<Expr>              callee,
		std::vector<base::Box<Expr>> arguments
	):
		  Expr(expression_type),
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
		return makeBox<CallExpr>(expression_type, callee->clone(), std::move(arguments_cloned));
	}

	AccessExpr::AccessExpr(query::Context&, Box<Expr> base, base::StrID field):
		  Expr(base->expression_type),
		  base(std::move(base)),
		  field(field) {}

	AccessExpr::AccessExpr(
		tsh::ExpressionType<> expression_type, base::Box<Expr> base, base::StrID field
	):
		  Expr(expression_type),
		  base(std::move(base)),
		  field(field) {}

	void AccessExpr::debugPrint(std::ostream& out) const {
		base->debugPrint(out);
		out << "." << field.str();
	}

	Box<Expr> AccessExpr::clone() const {
		return makeBox<AccessExpr>(expression_type, base->clone(), field);
	}

	SequenceExpr::SequenceExpr(query::Context&, std::vector<Box<Expr>> expressions):
		  Expr(expressions.back()->expression_type),
		  expressions(std::move(expressions)) {}

	SequenceExpr::SequenceExpr(
		tsh::ExpressionType<> expression_type, std::vector<base::Box<Expr>> expressions
	):
		  Expr(expression_type),
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
		return makeBox<SequenceExpr>(expression_type, std::move(expressions));
	}

	ChainComparisonExpr::ChainComparisonExpr(
		query::Context& ctx, std::vector<Box<Expr>> expressions, std::vector<BuiltinBinary> operators
	):
		  Expr(tsh::ExpressionType<>(
			  tsh::SymbolType{
				  ctx.query<tsh::QueryBoolType>({}),
				  tsh::ReferenceKind::Direct,
				  tsh::Mutability::Immutable,
			  },
			  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
		  )),
		  expressions{ std::move(expressions) },
		  operators{ std::move(operators) } {}

	ChainComparisonExpr::ChainComparisonExpr(
		tsh::ExpressionType<>        expression_type,
		std::vector<base::Box<Expr>> expressions,
		std::vector<BuiltinBinary>   operators
	):
		  Expr(expression_type),
		  expressions{ std::move(expressions) },
		  operators{ std::move(operators) } {}

	void ChainComparisonExpr::debugPrint(std::ostream& out) const {
		using namespace std::views;
		auto comparison_to_string = [](BuiltinBinary comp) {
			using enum BuiltinBinary;
			switch (comp) {
			case IntegerLt:
				return "<";
			case IntegerLteq:
				return "<=";
			case IntegerGt:
				return ">";
			case IntegerGteq:
				return ">=";
			case IntegerEq:
				return "==";
			case IntegerNeq:
				return "!=";
			default:
				CORE_UNREACHABLE();
			}
		};

		expressions.front()->debugPrint(out);
		for (auto [expr, comp]: zip(expressions | drop(1), operators)) {
			out << comparison_to_string(comp);
			expr->debugPrint(out);
		}
	}

	Box<Expr> ChainComparisonExpr::clone() const {
		std::vector<base::Box<Expr>> expressions;
		expressions.reserve(this->expressions.size());
		for (const auto& expr: this->expressions) expressions.push_back(expr->clone());
		return makeBox<ChainComparisonExpr>(expression_type, std::move(expressions), operators);
	}

}
