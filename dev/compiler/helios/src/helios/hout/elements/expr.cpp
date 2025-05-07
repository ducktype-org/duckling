/**
 * @file expr.cpp
 * @brief Implementation of methods in the Expr hierarchy.
 */

#include "expr.hpp"

#include "../visitors.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <query_framework/context.hpp>

namespace compiler::helios::code {

#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	EXPR_VISITOR(LiteralIntExpr)
	EXPR_VISITOR(LiteralBoolExpr)
	EXPR_VISITOR(LiteralTypeExpr)
	EXPR_VISITOR(IdentifierExpr)
	EXPR_VISITOR(BinaryOperatorExpr)
	EXPR_VISITOR(UnaryOperatorExpr)
	EXPR_VISITOR(TernaryOperatorExpr)
	EXPR_VISITOR(TupleTypeConstructorExpr)
	EXPR_VISITOR(VariantTypeConstructorExpr)
	EXPR_VISITOR(ParenthesisExpr)
	EXPR_VISITOR(LinkedIdentifierExpr)
	EXPR_VISITOR(CallExpr)

	LiteralIntExpr::LiteralIntExpr(query::Context& ctx, ScopeID scope, i64 value):
		  Expr(
			  scope,
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

	void LiteralIntExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

	LiteralBoolExpr::LiteralBoolExpr(query::Context& ctx, ScopeID scope, bool value):
		  Expr(
			  scope,
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

	void LiteralBoolExpr::debugPrint(std::ostream& out) const { out << (value ? "true" : "false"); }

	LiteralTypeExpr::LiteralTypeExpr(query::Context& ctx, ScopeID scope, tsh::AbstractType type):
		  Expr(
			  scope,
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

	void LiteralTypeExpr::debugPrint(std::ostream& out) const { out << value_type.toString(); }

	IdentifierExpr::IdentifierExpr(query::Context& ctx, ScopeID scope, SymID symbol):
		  Expr(
			  scope,
			  tsh::ExpressionType<>(
				  ctx.query<QueryTypeOfSymbol>(symbol)->expect(
					  "Handling errors in HOUT is not supported yet"
				  ),
				  tsh::ValueCategory(tsh::primaryCategoryOfSymbol(symbol))
			  )
		  ),
		  symbol(symbol) {}

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << strConcat("(Symbol ", name(symbol), " (", symbol.customPerfectHash(), "))");
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
			return ctx.query<tsh::QueryBoolType>({});
		case BooleanAnd:
		case BooleanOr:
			return argument_type;
		default:
			CORE_UNREACHABLE();
		}
	}

	BinaryOperatorExpr::BinaryOperatorExpr(
		query::Context& ctx, ScopeID scope, BuiltinBinary operation, Box<Expr> lhs, Box<Expr> rhs
	):
		  Expr(
			  scope,
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

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		// note: this might get more complex in the future

		lhs->debugPrint(out);
		switch (operation) {
		case BuiltinBinary::IntegerAdd:
			out << "+";
			break;
		case BuiltinBinary::IntegerSub:
			out << "-";
			break;
		case BuiltinBinary::IntegerMul:
			out << "*";
			break;
		case BuiltinBinary::IntegerDiv:
			out << "/";
			break;
		case BuiltinBinary::IntegerMod:
			out << "%";
			break;
		case BuiltinBinary::IntegerPow:
			out << "**";
			break;
		case BuiltinBinary::IntegerLt:
			out << "<";
			break;
		case BuiltinBinary::BooleanAnd:
			out << " and ";
			break;
		case BuiltinBinary::BooleanOr:
			out << " or ";
			break;
		}
		rhs->debugPrint(out);
	}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	ParenthesisExpr::ParenthesisExpr(query::Context&, ScopeID scope, Box<Expr> inner):
		  Expr(scope, inner->expression_type),
		  inner(std::move(inner)) {}

	TernaryOperatorExpr::TernaryOperatorExpr(
		query::Context&, ScopeID scope, Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false
	):
		  Expr(scope, if_true->expression_type),
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

	TupleTypeConstructorExpr::TupleTypeConstructorExpr(
		query::Context& ctx, ScopeID scope, std::vector<Box<Expr>> elements
	):
		  Expr(
			  scope,
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

	void TupleTypeConstructorExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_comma = false; auto&& e: elements) {
			if (add_comma) out << ", ";
			e->debugPrint(out);
			add_comma = true;
		}
		out << ")";
	}

	void VariantTypeConstructorExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_pipe = false; auto&& subtype: subtypes) {
			if (add_pipe) out << " | ";
			subtype->debugPrint(out);
			add_pipe = true;
		}
		out << ")";
	}

	VariantTypeConstructorExpr::VariantTypeConstructorExpr(
		query::Context& ctx, ScopeID scope, std::vector<Box<Expr>> subtypes
	):
		  Expr(
			  scope,
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

	void LinkedIdentifierExpr::debugPrint(std::ostream& out) const {
		for (bool add_dot = false; auto&& symbol: symbols) {
			if (add_dot) out << ".";
			out << name(symbol).str();
			add_dot = true;
		}
	}

	LinkedIdentifierExpr::LinkedIdentifierExpr(
		query::Context& ctx, ScopeID scope, SymbolList symbols
	):
		  Expr(
			  scope,
			  tsh::ExpressionType(
				  ctx.query<QueryTypeOfSymbol>(symbols.back())
					  ->expect("Not handling errors here yet"),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  symbols(std::move(symbols)) {}

	UnaryOperatorExpr::UnaryOperatorExpr(ScopeID scope, BuiltinUnary operation, Box<Expr> expr):
		  Expr(scope, expr->expression_type),
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
		}
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
		query::Context& ctx, ScopeID scope, SymID callee, std::vector<Box<Expr>> arguments
	):
		  Expr(
			  scope,
			  tsh::ExpressionType(
				  getCallResultType(ctx.query<QueryTypeOfSymbol>(callee)
	                                    ->expect(strConcat("Calling invalid symbol: ", name(callee)))
	                                    .getType()),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  )
		  ),
		  callee(callee),
		  arguments(std::move(arguments)) {}

	void CallExpr::debugPrint(std::ostream& out) const {
		out << name(callee).strView() << "(";
		bool add_comma = false;
		for (auto&& arg: arguments) {
			if (add_comma) out << ", ";
			arg->debugPrint(out);
			add_comma = true;
		}
		out << ")";
	}
}
