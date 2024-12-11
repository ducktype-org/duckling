#include <query_framework/query_impl.hpp>
#include "query_hout_of_expr.hpp"

namespace compiler::helios {
	struct
		IMPLEMENT_QUERY(QueryHoutOfExpr, errors::HResult<base::Box<code::Expr> COMMA errors::Failed>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			return code::Expr::fromPST(ctx, key.expr);
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)

	base::HashT KeyOf_QueryHoutOfExpr::customPerfectHash() const {
		auto hash_1 = this->expr->getID().asInt();

		return hash_1;
	}
}
