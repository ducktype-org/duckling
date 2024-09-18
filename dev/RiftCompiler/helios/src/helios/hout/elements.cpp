#include "elements.hpp"

#include <query_framework/query_impl.hpp>
#include <base/variant.hpp>
#include "../scopes/scopes.hpp"
#include <base/unique_pointer.hpp>
#include <helios/hout/element_ref.hpp>
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

	void LiteralValueExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << base::strConcat("(Symbol ", symbol.customPerfectHash(), ")");
	}

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		out << base::strConcat("(");
		lhs->debugPrint(out);
		out << base::strConcat(op);
		rhs->debugPrint(out);
		out << base::strConcat(")");
	}

	void BinaryOperatorExpr::acceptVisitor(HoutExprVisitor& visitor) const {
		visitor.visitBinaryOperatorExpr(*this);
	}

	ElementRef<Expr> Expr::fromRPN(query::Context& ctx, const rpn::RPNExpr& expr) {
		// The algorithm from RPN: https://en.wikipedia.org/wiki/Binary_expression_tree
		std::stack<ElementRef<Expr>> st;
		for (auto&& elem: expr.elements) {
			variant_match(elem) {
				variant_case(rpn::Identifier, idt) {
					st.emplace(
						base::make_unique<IdentifierExpr>(expr.scope, idt.symbol_list.back(), ctx)
					);
				}

				variant_case(rpn::Operator, oper) {
					auto b = std::move(st.top());
					st.pop();
					auto a = std::move(st.top());
					st.pop();

					st.emplace(base::make_unique<BinaryOperatorExpr>(
						expr.scope, oper.oper_id, std::move(a), std::move(b), ctx
					));
				}

				variant_case(rpn::NamedIdentifier, idt) {
					auto&& sym_list = ctx.query<QueryLookupInScopeAndParents>(
						{ expr.scope, idt.symbol_name, true }
					);

					st.emplace(base::make_unique<IdentifierExpr>(
						expr.scope, sym_list.getAsSingle().back(), ctx
					));
				}

				variant_case(rpn::NumValue, num_value) {
					st.emplace(base::make_unique<LiteralValueExpr>(
						expr.scope, std::stoi(num_value.num_id.str()), ctx
					));
				}

				variant_default {
					RIFT_PANIC(base::strConcat(
						"Unhandlable type during parsing type from expr: ", typeid(elem).name()
					));
				}
			}
		}
		RIFT_ASSERT(st.size() == 1, "Empty HOUT Tree stack");
		return std::move(st.top());
	}

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
}

namespace compiler::helios {

	// @NOTE: code for creating HoutOfExpr is adapted from HIR, and is generally temporary

	/**
	 * @brief HoutOfExpr for expression that contain only one element
	 */
	auto houtOfSingleExpr(query::Context& ctx, KeyOf_QueryHoutOfExpr key)
		-> code::ElementRef<code::Expr> {
		auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ key.expr });

		RIFT_ASSERT(key.expr->elements.size() == 1, "houtOfSingleExpr got non single expression");
		const auto& elem = key.expr->elements.at(0);

		variant_match(elem) {
			variant_case(pst::Expr::KeywordValue, key) {
				throw base::NotYetImplemented("Keyword expressions");
			}
			variant_case(pst::Expr::NumLiteral, num) {
				auto val = base::strIDToNum(num.num_id);
				return base::make_unique<code::LiteralValueExpr>(scope, val, ctx);
			}
			variant_case(pst::Expr::Identifier, identifier) {
				// @note: this does not handle overload
				// @note: this does not handle "." operation

				auto lookup_result = ctx.query<QueryLookupInScopeAndParents>(KeyOf_LookupInScope{
					scope, identifier.indent_id, true });

				compiler::helios::SymbolList lookup_dealiased;

				auto symbol_path = lookup_result.getAsSingle();

				for (auto single_sym: symbol_path) {
					auto dealiased = ctx.query<compiler::helios::QueryDealias>(single_sym);
					lookup_dealiased.insert(
						lookup_dealiased.end(), dealiased.begin(), dealiased.end()
					);
				}

				RIFT_ASSERT(lookup_dealiased.size() > 0, "Empty lookup result");

				// @TODO: dont just ignore everything before last symbol
				return base::make_unique<code::IdentifierExpr>(scope, lookup_dealiased.back(), ctx);
			}
			variant_case(pst::Expr::Group, group) {
				throw base::NotYetImplemented("Expr from group");
			}
			variant_case_novalue(pst::Expr::Operator) {
				RIFT_PANIC("Expression consisting of only operator is not allowed (yet?).");
			}
		}
		RIFT_PANIC("No match in variant");
	}

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, code::ElementRef<code::Expr>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @NOTE: this is simplest, mock implementation
			// A proper Expr parsing will be added as new mission/PR

			if (key.expr->elements.size() == 1)
				return houtOfSingleExpr(ctx, key);
			else {
				std::stringstream expr_dprint;
				key.expr->dprint(expr_dprint);
				throw base::NotYetImplemented(
					base::strConcat("Complicated HOUT expressions: ", expr_dprint.str())
				);
			}
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
