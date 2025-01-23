/**
 * @file expr.cpp
 * @brief Implementation of methods in the Expr hierarchy.
 */

#include "expr.hpp"
#include "../visitors.hpp"

#include <query_framework/query_impl.hpp>

namespace compiler::helios::code {

#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	EXPR_VISITOR(LiteralIntExpr)
	EXPR_VISITOR(LiteralBoolExpr)
	EXPR_VISITOR(LiteralTypeExpr)
	EXPR_VISITOR(IdentifierExpr)
	EXPR_VISITOR(BinaryOperatorExpr)
	EXPR_VISITOR(UnaryOperatorExpr)
	EXPR_VISITOR(TupleTypeConstructorExpr)
	EXPR_VISITOR(VariantTypeConstructorExpr)
	EXPR_VISITOR(ParenthesisExpr)
	EXPR_VISITOR(LinkedIdentifierExpr)

	LiteralIntExpr::LiteralIntExpr(query::Context& ctx, ScopeID scope, i64 value):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  // @TODO: Select type of expression based on type of literal.
				  ctx.query<tsh::QueryIntegralType>({ 64 }),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	void LiteralIntExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

	LiteralBoolExpr::LiteralBoolExpr(query::Context& ctx, ScopeID scope, bool value):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  ctx.query<tsh::QueryBoolType>({}),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	void LiteralBoolExpr::debugPrint(std::ostream& out) const { out << (value ? "true" : "false"); }

	LiteralTypeExpr::LiteralTypeExpr(query::Context& ctx, ScopeID scope, tsh::TypeInfo type):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  ctx.query<tsh::QueryMetaType>({}),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value_type(type) {}

	void LiteralTypeExpr::debugPrint(std::ostream& out) const { out << value_type.toString(); }

	IdentifierExpr::IdentifierExpr(query::Context& ctx, ScopeID scope, SymID symbol):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  ctx.query<QueryTypeOfSymbol>(symbol)->expect(
					  "Handling errors in HOUT is not supported yet"
				  ),
				  tsh::ValueCategory(tsh::primaryCategoryOfSymbol(symbol))
			  )
		  ),
		  symbol(symbol) {}

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << base::strConcat("(Symbol ", name(symbol), " (", symbol.customPerfectHash(), "))");
	}

	BinaryOperatorExpr::BinaryOperatorExpr(
		query::Context& ctx,
		ScopeID         scope,
		lexer::Operator op,
		base::Box<Expr> lhs,
		base::Box<Expr> rhs
	):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  // @TODO: Select type of expression based on result type of the operation.
				  ctx.query<tsh::QueryIntegralType>({ 64 }),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  )
		  ),
		  op(op),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		lhs->debugPrint(out);
		out << base::strConcat(op.str());
		rhs->debugPrint(out);
	}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	ParenthesisExpr::ParenthesisExpr(query::Context&, ScopeID scope, base::Box<Expr> inner):
		  Expr(scope, inner->type_desc),
		  inner(std::move(inner)) {}

	TupleTypeConstructorExpr::TupleTypeConstructorExpr(
		query::Context& ctx, ScopeID scope, std::vector<base::Box<Expr>> elements
	):
		  Expr(scope, ctx.query<tsh::QueryMetaType>({})),
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
		query::Context& ctx, ScopeID scope, std::vector<base::Box<Expr>> subtypes
	):
		  Expr(scope, ctx.query<tsh::QueryMetaType>({})),
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
			  tsh::TypeDesc<>(
				  ctx.query<QueryTypeOfSymbol>(symbols.back())
					  ->expect("Not handling errors here yet"),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  symbols(std::move(symbols)) {}

	UnaryOperatorExpr::UnaryOperatorExpr(
		ScopeID scope, lexer::Operator op, bool prefix, base::Box<Expr> expr
	):
		  Expr(scope, expr->type_desc),
		  op(op),
		  prefix(prefix),
		  expr(std::move(expr)) {}

	void UnaryOperatorExpr::debugPrint(std::ostream& out) const {
		if (prefix) {
			out << op.str();
			expr->debugPrint(out);
		} else {
			expr->debugPrint(out);
			out << op.str();
		}
	}
}
