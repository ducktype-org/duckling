#include "elements.hpp"

#include <query_framework/query_impl.hpp>
#include <base/variant.hpp>
#include "../scopes/scopes.hpp"
#include "../symbols/symbols.hpp"

namespace compiler::helios::code {

	void addIndent(usize indent, std::string& out) { out.append(indent * 4, ' '); }

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

	void ConstIntExprMock::debugPrint(std::string& out) const { out += std::to_string(value); }

	void IdentifierExpresion::debugPrint(std::string& out) const {
		out += base::strConcat("(Symbol ", symbol.customPerfectHash(), ")");
	}

}

namespace compiler::helios {

	// @NOTE: code for creating HoutOfExpr is adapted from HIR, and is generally temporary

	/**
	 * @brief HoutOfExpr for expression that contain only one element
	 */
	QUERY_EXTENSION(houtOfSingleExpr, KeyOf_QueryHoutOfExpr, code::ElementRef<code::Expr>);

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
				return base::make_unique<code::ConstIntExprMock>(val);
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
				return base::make_unique<code::IdentifierExpresion>(lookup_dealiased.back());
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
				return ctx.callExt<houtOfSingleExpr>(key);
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
