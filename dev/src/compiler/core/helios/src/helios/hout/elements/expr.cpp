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

	void LiteralIntExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

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

	void LiteralBoolExpr::debugPrint(std::ostream& out) const { out << (value ? "true" : "false"); }

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

	void LiteralStringExpr::debugPrint(std::ostream& out) const { out << value.str(); }

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

	void LiteralTypeExpr::debugPrint(std::ostream& out) const { out << value_type.toString(); }

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

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << strConcat("(Symbol ", name(symbol), " (", symbol.queryUnstablePerfectHash(), "))");
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

		case IntegerLt:
		case IntegerLteq:
		case IntegerGt:
		case IntegerGteq:
		case IntegerEq:
		case IntegerNeq:
			CORE_PANIC("comparisons should be handled by ComparisonChain, not BinaryOperator");
		}
		rhs->debugPrint(out);
	}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	ParenthesisExpr::ParenthesisExpr(query::Context&, Box<Expr> inner):
		  Expr(inner->expression_type),
		  inner(std::move(inner)) {}

	TernaryOperatorExpr::TernaryOperatorExpr(
		query::Context&, Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false
	):
		  Expr(if_true->expression_type),
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

	UnaryOperatorExpr::UnaryOperatorExpr(BuiltinUnary operation, Box<Expr> expr):
		  Expr(expr->expression_type),
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
		default:
			CORE_PANIC("unsupported unary operation");
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

	CallExpr::CallExpr(query::Context&, base::Box<Expr> callee, std::vector<Box<Expr>> arguments):
		  Expr(tsh::ExpressionType(
			  getCallResultType(callee->expression_type.getType()),
			  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
		  )),
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

	AccessExpr::AccessExpr(query::Context&, Box<Expr> base, base::StrID field):
		  Expr(base->expression_type),
		  base(std::move(base)),
		  field(field) {}

	void AccessExpr::debugPrint(std::ostream& out) const {
		base->debugPrint(out);
		out << "." << field.str();
	}

	SequenceExpr::SequenceExpr(query::Context&, std::vector<Box<Expr>> expressions):
		  Expr(expressions.back()->expression_type),
		  expressions(std::move(expressions)) {}

	void SequenceExpr::debugPrint(std::ostream& out) const {
		for (bool add_comma = false; auto&& expr: expressions) {
			if (add_comma) out << ", ";
			expr->debugPrint(out);
			add_comma = true;
		}
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
}
