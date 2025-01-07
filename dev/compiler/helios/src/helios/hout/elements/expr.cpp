/**
 * @file expr.cpp
 * @brief Implementation of methods in the Expr hierarchy.
 */

#include "expr.hpp"
#include "../visitors.hpp"

#include <query_framework/query_impl.hpp>

namespace compiler::helios::code {
	namespace {
		tsh::TypeInfo getTypeOfKeyword(query::Context& ctx, lang_def::Keyword keyword) {
			// @todo HOUT 2.0 this is incorrect
			const static auto BUILTINS = std::unordered_map<lang_def::Keyword, tsh::TypeInfo>{
				{ lang_def::Keyword::f80, ctx.query<::tsh::QueryFloatType>(80) },
				{ lang_def::Keyword::f64, ctx.query<::tsh::QueryFloatType>(64) },
				{ lang_def::Keyword::f32, ctx.query<::tsh::QueryFloatType>(32) },
				{ lang_def::Keyword::f16, ctx.query<::tsh::QueryFloatType>(16) },

				{ lang_def::Keyword::i128, ctx.query<::tsh::QueryIntegralType>({ 128, true }) },
				{ lang_def::Keyword::i64, ctx.query<::tsh::QueryIntegralType>({ 64, true }) },
				{ lang_def::Keyword::i32, ctx.query<::tsh::QueryIntegralType>({ 32, true }) },
				{ lang_def::Keyword::i16, ctx.query<::tsh::QueryIntegralType>({ 16, true }) },
				{ lang_def::Keyword::i8, ctx.query<::tsh::QueryIntegralType>({ 8, true }) },

				{ lang_def::Keyword::u128, ctx.query<::tsh::QueryIntegralType>({ 128, false }) },
				{ lang_def::Keyword::u64, ctx.query<::tsh::QueryIntegralType>({ 64, false }) },
				{ lang_def::Keyword::u32, ctx.query<::tsh::QueryIntegralType>({ 32, false }) },
				{ lang_def::Keyword::u16, ctx.query<::tsh::QueryIntegralType>({ 16, false }) },
				{ lang_def::Keyword::u8, ctx.query<::tsh::QueryIntegralType>({ 8, false }) },

				{ lang_def::Keyword::Bool, ctx.query<::tsh::QueryBoolType>({}) },
			};
			return BUILTINS.at(keyword);
		}
	}

#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	EXPR_VISITOR(LiteralValueExpr)
	EXPR_VISITOR(IdentifierExpr)
	EXPR_VISITOR(BinaryOperatorExpr)
	EXPR_VISITOR(UnaryOperatorExpr)
	EXPR_VISITOR(TupleTypeConstructorExpr)
	EXPR_VISITOR(VariantTypeConstructorExpr)
	EXPR_VISITOR(ParenthesisExpr)
	EXPR_VISITOR(KeywordExpr)
	EXPR_VISITOR(LinkedIdentifierExpr)

	LiteralValueExpr::LiteralValueExpr(query::Context& ctx, ScopeID scope, i64 value):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  // @TODO: Select type of expression based on type of literal.
				  ctx.query<tsh::QueryIntegralType>({ 64 }),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	void LiteralValueExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

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
		BuiltinBinary   operation,
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
		  operation(operation),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		// hmm, @todo in this PR
		// lhs->debugPrint(out);
		// out << base::strConcat(op.str());
		// rhs->debugPrint(out);
	}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	ParenthesisExpr::ParenthesisExpr(query::Context&, ScopeID scope, base::Box<Expr> inner):
		  Expr(scope, inner->type_desc),
		  inner(std::move(inner)) {}

	KeywordExpr::KeywordExpr(query::Context& ctx, ScopeID scope, lang_def::Keyword keyword):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  getTypeOfKeyword(ctx, keyword), tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  keyword(keyword) {}

	void KeywordExpr::debugPrint(std::ostream& out) const {
		out << lang_def::keywordToStr(keyword).strView();
	}

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
		ScopeID scope, BuiltinUnary operation, base::Box<Expr> expr
	):
		  Expr(scope, expr->type_desc),
		  operation(operation),
		  expr(std::move(expr)) {}

	void UnaryOperatorExpr::debugPrint(std::ostream& out) const {
		// hmm, @todo in this PR
		// if (prefix) {
		// 	out << op.str();
		// 	expr->debugPrint(out);
		// } else {
		// 	expr->debugPrint(out);
		// 	out << op.str();
		// }
	}
}
