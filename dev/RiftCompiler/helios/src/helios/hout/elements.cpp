#include "elements.hpp"

#include <query_framework/query_impl.hpp>
#include <base/variant.hpp>

namespace compiler::helios::code {

	void addIndent(usize indent, std::string& out) {
		out.append(indent * 4, ' ');
	}

	void ReturnStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "return [@TODO]\n";
	}

	void VReturnStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "void return\n";
	}
}

namespace compiler::helios {
	
	// @NOTE: code for creating HoutOfExpr is adapted from HIR, and is generally temporary

	/**
	 * @brief HoutOfExpr for expression that contain only one element
	 */
	QUERY_EXTENSION(houtOfSingleExpr, KeyOf_HoutOfExpr, code::ElementRef<code::Expr>);
	auto houtOfSingleExpr(query::Context&, KeyOf_HoutOfExpr key) -> code::ElementRef<code::Expr> {
		RIFT_ASSERT(key.expr->elements.size() == 1, "houtOfSingleExpr got non single expression");
		auto&& elem = key.expr->elements.at(1);

		variant_match(elem) {
			variant_case(pst::Expr::KeywordValue, key) {
				throw base::NotYetImplemented("Keyword expressions");
			}
			variant_case(pst::Expr::NumLiteral, num) {
				auto val = base::strIdToNum(num.num_id);
				return base::make_unique<code::ConstIntExpr>(val);
			}
			variant_case(pst::Expr::Identifier, identifier) {
				throw base::NotYetImplemented("Identifier expressions");
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

	struct IMPLEMENT_QUERY(HoutOfExpr, code::ElementRef<code::Expr>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			if (key.expr->elements.size() == 1) {
				return ctx.callExt<houtOfSingleExpr>(key);
			}
			else {
				throw base::NotYetImplemented("Complicated HOUT expressions");
			}
		}

		static auto load(QKey) -> LoadResult {
			return {};
		}

		static auto store(QKey, PResult res, query::ACD) -> QResult {
			return res;
		}
	};
};
