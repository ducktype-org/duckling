#include "elements.hpp"

#include <cmath>
#include <query_framework/query_impl.hpp>
#include <base/variant.hpp>
#include "../scopes/scopes.hpp"
#include <base/unique_pointer.hpp>
#include "base/box.hpp"
#include "base/exceptions.hpp"
#include "base/optional.hpp"
#include "helios/helios_errors.hpp"
#include "helios/helios_result.hpp"
#include "helios/scope_symbol_id.hpp"
#include "helios/symbols/symbols.hpp"
#include "pst_parser/elements/hierarchy/not_statements.hpp"
#include "pst_parser/pst_expr_visitor.hpp"
#include "query_framework/query_int.hpp"
#include "visitors.hpp"

namespace compiler::helios::code {
	constexpr usize INDENT_SIZE = 4;

	void addIndent(std::ostream& out, usize indent) {
		out << std::string().append(indent * INDENT_SIZE, ' ');
	}

	void ReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "return ";
		this->value->debugPrint(out);
		out << "\n";
	}

	void VoidReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "void return\n";
	}

	void ExprStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "do ";
		expr->debugPrint(out);
		out << "\n";
	}

	void IfStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "if (";
		condition->debugPrint(out);
		out << ") {\n";
		for (const auto& stmt: body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "}\n";
	}

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

	void VariableStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "var ";
		out << name(this->helios_symbol).strView();
		out << " : ";

		// this might not be correct:?
		out << this->type.getType().toString();
		out << " = ";
		this->initial_value.value()->debugPrint(out);

		out << ";\n";
	}

	IdentifierExpr::IdentifierExpr(query::Context& ctx, ScopeID scope, SymID symbol):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  ctx.query<QueryTypeOfSymbol>(symbol).expect(
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

	void BinaryOperatorExpr::acceptVisitor(HoutExprVisitor& visitor) const {
		visitor.visitBinaryOperatorExpr(*this);
	}

	struct PstExprToHoutExprVisitor: public pst::PstExprVisitorPanicky {
		explicit PstExprToHoutExprVisitor(query::Context& ctx, ScopeID scope):
			  ctx(ctx),
			  scope(scope) {}

		query::Context& ctx;
		ScopeID         scope;

		base::MBox<Expr> node = nullptr;

		void visitExprValue(const pst::expr::ExprValue& stmt) override {
			stmt.getValue();
			// @TODO: Change literal value from i64 to something more appropriate.
			node = base::MBox(new LiteralValueExpr(ctx, scope, std::stoi(stmt.getValue().str())));
		}

		void visitBinaryOperator(const pst::expr::BinaryOperator& stmt) override {
			PstExprToHoutExprVisitor lhs(ctx, scope);
			PstExprToHoutExprVisitor rhs(ctx, scope);
			stmt.getLeftOperand()->acceptVisitor(lhs);
			stmt.getRightOperand()->acceptVisitor(rhs);
			CORE_ASSERT(lhs.node, "Invalid LHS of binary operator");
			CORE_ASSERT(rhs.node, "Invalid RHS of binary operator");
			node = base::MBox(new BinaryOperatorExpr(
				ctx,
				scope,
				stmt.getOperator(),
				std::move(lhs.node).toOptBox().value(),
				std::move(rhs.node).toOptBox().value()
			));
		}

		void visitRoundExpr(const pst::expr::RoundExpr& stmt) override {
			PstExprToHoutExprVisitor vis(ctx, scope);
			stmt.getInner()->acceptVisitor(vis);
			CORE_ASSERT(vis.node, "Invalid inner expr of RoundExpr");
			node
				= base::MBox(new ParenthesisExpr(ctx, scope, std::move(vis.node).toOptBox().value())
			    );
		}

		void visitIdentifierLiteral(const pst::expr::IdentifierLiteral& stmt) override {
			auto&& sym_list
				= ctx.query<QueryLookupInScopeAndParents>({ scope, stmt.getName().value, true });
			auto single = sym_list.getAsSingle().expect("Not propagating errors here yet...");

			node = base::MBox(new IdentifierExpr(ctx, scope, single.back()));
		}
	};

	errors::HResult<base::Box<Expr>, errors::Failed>
		Expr::fromPST(query::Context& ctx, ScopeID scope, const PstRef<pst::ExprElement> root) {
		PstExprToHoutExprVisitor visitor(ctx, scope);
		std::cerr << "\nExpr: \n";
		root->debugPrint(std::cerr);
		std::cerr << '\n';
		root->acceptVisitor(visitor);
		auto opt_box = std::move(visitor.node).toOptBox();
		if (opt_box.has_value()) return std::move(opt_box.value());
		return errors::HError(errors::Failed());
	}

	// base::Box<Expr> Expr::fromRPN(query::Context& ctx, const rpn::RPNExpr& expr) {
	// 	// The algorithm from RPN: https://en.wikipedia.org/wiki/Binary_expression_tree
	// 	std::stack<base::Box<Expr>> st;
	// 	for (auto&& elem: expr.elements) {
	// 		variant_match(elem) {
	// 			variant_case(rpn::Identifier, idt) {
	// 				st.emplace(
	// 					base::make_unique<IdentifierExpr>(expr.scope, idt.symbol_list.back(), ctx)
	// 				);
	// 			}

	// 			variant_case(rpn::Operator, oper) {
	// 				auto b = std::move(st.top());
	// 				st.pop();
	// 				auto a = std::move(st.top());
	// 				st.pop();

	// 				st.emplace(base::make_unique<BinaryOperatorExpr>(
	// 					expr.scope, oper.oper_id, std::move(a), std::move(b), ctx
	// 				));
	// 			}

	// 			variant_case(rpn::NamedIdentifier, idt) {
	// 				auto&& sym_list = ctx.query<QueryLookupInScopeAndParents>(
	// 					{ expr.scope, idt.symbol_name, true }
	// 				);
	// 				auto single
	// 					= sym_list.getAsSingle().expect("Not propagating errors here yet...");
	// 				st.emplace(base::make_unique<IdentifierExpr>(expr.scope, single.back(), ctx));
	// 			}

	// 			variant_case(rpn::NumValue, num_value) {
	// 				st.emplace(base::make_unique<LiteralValueExpr>(
	// 					expr.scope, std::stoi(num_value.num_id.str()), ctx
	// 				));
	// 			}

	// 			variant_default {
	// 				CORE_PANIC(base::strConcat(
	// 					"Unhandlable type during parsing type from expr: ", typeid(elem).name()
	// 				));
	// 			}
	// 		}
	// 	}
	// 	CORE_ASSERT(st.size() == 1, "Empty HOUT Tree stack");
	// 	return std::move(st.top());
	// }

// visitors:
#define STMT_VISITOR(type) \
	void type::acceptVisitor(HoutStmtVisitor& visitor) const { visitor.visit##type(*this); }
#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	STMT_VISITOR(ReturnStmt);
	STMT_VISITOR(VoidReturnStmt);
	STMT_VISITOR(ExprStmt);
	STMT_VISITOR(IfStmt);
	STMT_VISITOR(VariableStmt);

	EXPR_VISITOR(LiteralValueExpr);
	EXPR_VISITOR(IdentifierExpr);
	EXPR_VISITOR(ParenthesisExpr);

	errors::HResult<i64, errors::Failed> LiteralValueExpr::evaluateValue(query::Context& ctx
	) const {
		return value;
	}

	errors::HResult<i64, errors::Failed> BinaryOperatorExpr::evaluateValue(query::Context& ctx
	) const {
		UNPACK_RESULT(i64 lhs_value =, lhs->evaluateValue(ctx));
		UNPACK_RESULT(i64 rhs_value =, rhs->evaluateValue(ctx));
		if (op.str()[0] == '+') return lhs_value + rhs_value;
		if (op.str()[0] == '-') return lhs_value - rhs_value;
		if (op.str()[0] == '*') return lhs_value * rhs_value;
		if (op.str()[0] == '/') return lhs_value / rhs_value;
		if (op.str()[0] == '%') return lhs_value % rhs_value;
		if (op.str()[0] == '^') return std::pow(lhs_value, rhs_value);
		CORE_PANIC("Unknown operator");
	}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	ParenthesisExpr::ParenthesisExpr(query::Context& ctx, ScopeID scope, base::Box<Expr> inner):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  // @TODO: Select type of expression based on result type of the operation.
				  ctx.query<tsh::QueryIntegralType>({ 64 }),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  )
		  ),
		  inner(std::move(inner)) {}

	errors::HResult<i64, errors::Failed> ParenthesisExpr::evaluateValue(query::Context& ctx) const {
		return inner->evaluateValue(ctx);
	}

	errors::HResult<i64, errors::Failed> IdentifierExpr::evaluateValue(query::Context& ctx) const {
		return ctx.query<QueryConstValueOf>(symbol);
	}
}

namespace compiler::helios {


	/**
	 * @brief HoutOfExpr for expression that contain only one element
	 */
	auto houtOfSingleExpr(query::Context& ctx, KeyOf_QueryHoutOfExpr key) -> base::Box<code::Expr> {
		CORE_PANIC("Not implemented yet...");

		// auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ key.expr });

		// CORE_ASSERT(key.expr->elements.size() == 1, "houtOfSingleExpr got non single
		// expression"); const auto& elem = key.expr->elements.at(0);

		// variant_match(elem) {
		// 	variant_case(pst::Expr::KeywordValue, key) {
		// 		throw base::NotYetImplemented("Keyword expressions");
		// 	}
		// 	variant_case(pst::Expr::NumLiteral, num) {
		// 		auto val = base::strIDToNum(num.num_id);
		// 		return base::make_unique<code::LiteralValueExpr>(scope, val, ctx);
		// 	}
		// 	variant_case(pst::Expr::Identifier, identifier) {
		// 		// @note: this does not handle overload
		// 		// @note: this does not handle "." operation

		// 		auto lookup_result = ctx.query<QueryLookupInScopeAndParents>(KeyOf_LookupInScope{
		// 			scope, identifier.indent_id, true });

		// 		compiler::helios::SymbolList lookup_dealiased;

		// 		auto symbol_path
		// 			= lookup_result.getAsSingle().expect("Not propagating errors for now...");

		// 		for (auto single_sym: symbol_path) {
		// 			auto dealiased = ctx.query<compiler::helios::QueryDealias>(single_sym)
		// 			                     .expect("Not propagating errors for now...");
		// 			lookup_dealiased.insert(
		// 				lookup_dealiased.end(), dealiased.begin(), dealiased.end()
		// 			);
		// 		}

		// 		CORE_ASSERT(!lookup_dealiased.empty(), "Empty lookup result");

		// 		// @TODO: dont just ignore everything before last symbol
		// 		return base::make_unique<code::IdentifierExpr>(scope, lookup_dealiased.back(), ctx);
		// 	}
		// 	variant_case(pst::Expr::Group, group) {
		// 		throw base::NotYetImplemented("Expr from group");
		// 	}
		// 	variant_case_novalue(pst::Expr::Operator) {
		// 		CORE_PANIC("Expression consisting of only operator is not allowed (yet?).");
		// 	}
		// }
		// CORE_PANIC("No match in variant");
	}

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, base::Box<code::Expr>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @NOTE: this is simplest, mock implementation
			// A proper Expr parsing will be added as new mission/PR

			CORE_PANIC("Not implemented yet...");
			// if (key.expr->elements.size() == 1)
			// 	return houtOfSingleExpr(ctx, key);
			// else {
			// 	std::stringstream expr_dprint;
			// 	key.expr->dprint(expr_dprint);
			// 	throw base::NotYetImplemented(
			// 		base::strConcat("Complicated HOUT expressions: ", expr_dprint.str())
			// 	);
			// }
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr);

	base::HashT KeyOf_QueryHoutOfExpr::customPerfectHash() const {
		auto hash_1 = this->expr->getID().asInt();

		return hash_1;
	}

};
