#include "elements.hpp"

#include <query_framework/query_impl.hpp>
#include <base/variant.hpp>
#include "../scopes/scopes.hpp"
#include "../symbols/symbols.hpp"
#include <base/unique_pointer.hpp>
#include <helios/hout/element_ref.hpp>
#include "visitors.hpp"

namespace compiler::helios::code {

	constexpr usize INDENT_SIZE = 4;

	void addIndent(usize indent, std::string& out) { out.append(indent * INDENT_SIZE, ' '); }

	void ReturnStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "return ";
		this->value->debugPrint(out);
		out += "\n";
	}

	void VoidReturnStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "void return\n";
	}

	void ExprStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "do ";
		expr->debugPrint(out);
		out += "\n";
	}

	void IfStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "if (";
		condition->debugPrint(out);
		out += ") {\n";
		for (const auto& stmt: body.statements) stmt->debugPrint(indent + 1, out);
		addIndent(indent, out);
		out += "}\n";
	}

	void LiteralValueExpr::debugPrint(std::string& out) const { out += std::to_string(value); }

	void IdentifierExpr::debugPrint(std::string& out) const {
		out += base::strConcat("(Symbol ", symbol.customPerfectHash(), ")");
	}

	void BinaryOperatorExpr::debugPrint(std::string& out) const {
		out += base::strConcat("(");
		lhs->debugPrint(out);
		out += base::strConcat(op);
		rhs->debugPrint(out);
		out += base::strConcat(")");
	}

	void BinaryOperatorExpr::acceptVisitor(HoutExprVisitor& visitor) const {
		visitor.visitBinaryOperatorExpr(*this);
	}

	ElementRef<Expr> Expr::fromRPN(const std::vector<rpn::ExprElem>& elements) {
		std::stack<ElementRef<Expr>> st;
		for (auto&& elem: elements) {
			variant_match(elem) {
				variant_case(rpn::Identifier, idt) {
					st.push(base::make_unique<IdentifierExpr>(idt.symbol_list.back()));
				}
				variant_case(rpn::Operator, oper) {
					auto b = std::move(st.top());
					st.pop();
					auto a = std::move(st.top());
					st.pop();
					st.push(base::make_unique<BinaryOperatorExpr>(
						oper.oper_id, std::move(a), std::move(b)
					));
				}
				variant_case(rpn::NamedIdentifier, named_identifier) {
					// todo: Write lookup? not really
					// st.push(base::make_unique<Expr>(IdentifierExpr(idt.symbol_list.back())));
				}
				variant_case(rpn::KeywordValue, keyword_val) {
					std::cout << keywordToStr(keyword_val.keyword).str() << '\n';
				}
				variant_case(rpn::NumValue, num_value) {
					st.push(base::make_unique<LiteralValueExpr>(std::stoi(num_value.num_id.str())));
				}
				// variant_case(rpn::TupleType, tuple_type) {}
				// variant_case(rpn::Variant, variant_type) {}
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
		RIFT_ASSERT(key.expr->elements.size() == 1, "houtOfSingleExpr got non single expression");
		const auto& elem = key.expr->elements.at(0);

		variant_match(elem) {
			variant_case(pst::Expr::KeywordValue, key) {
				throw base::NotYetImplemented("Keyword expressions");
			}
			variant_case(pst::Expr::NumLiteral, num) {
				auto val = base::strIdToNum(num.num_id);
				return base::make_unique<code::LiteralValueExpr>(val);
			}
			variant_case(pst::Expr::Identifier, identifier) {
				// @note: this does not handle overload
				// @note: this does not handle "." operation

				auto lookup_result = ctx.query<QueryLookupInScopeAndParents>(KeyOf_LookupInScope{
					key.scope, identifier.indent_id, true });

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
				return base::make_unique<code::IdentifierExpr>(lookup_dealiased.back());
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
			else
				throw base::NotYetImplemented("Complicated HOUT expressions");
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
		auto hash_1 = base::perfectHash(scope);
		auto hash_2 = this->expr->getID().asInt();

		// @FIXME: this does not work:
		return (hash_1 * 143 + hash_2 * 7);
	}

};
