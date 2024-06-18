#include <query_framework/query_impl.hpp>

/**
 * @brief Query to get the size of a type.
 *
 * @note Prefer to use the TypeInfo::getSize method directly for efficiency.
 * This query is for access through a query::entryPoint.
 */
DECLARE_QUERY(QuerySizeOfType, ts::TypeInfo, usize)

struct IMPLEMENT_QUERY(QuerySizeOfType, usize) {
	static auto provide(Context& ctx, QKey key) -> PResult { return key.getSize(ctx); }

	static auto load(QKey) -> LoadResult { return {}; }

	static auto store(QKey, const PResult p_res, query::ACD) -> QResult { return QResult{ p_res }; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(QuerySizeOfType)
