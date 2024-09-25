#include "queries.hpp"

#include <query_framework/query_impl.hpp>

namespace tsl {
	struct IMPLEMENT_QUERY(QueryTypeLayout, TypeLayout) {
		static auto provide(Context&, const QKey& key) -> PResult {
			using enum tsh::Kind;
			switch(key.getKind()) {
			case Unit: return EmptyLayout(key);
			case Integral: return IntegralTypeLayout(tsh::IntegralInfo(key));
			default:
				RIFT_PANIC("Nooo!");
			}
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeLayout)
}
