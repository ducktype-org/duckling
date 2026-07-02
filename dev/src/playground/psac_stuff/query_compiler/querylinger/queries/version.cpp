#include "version.hpp"

#include <query_framework/standard_query/query_impl.hpp>

#include <atomic>

namespace ql {
	namespace {
		std::atomic<usize> provide_calls{ 0 };
	}

	struct IMPLEMENT_QUERY(QueryCompilerVersion, std::string) {
		static auto provide(query::Context& /*ctx*/, QKey /*key*/) -> PResult {
			provide_calls.fetch_add(1, std::memory_order_relaxed);
			return std::string(QUERYLINGER_VERSION);
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCompilerVersion);

	usize versionProvideCalls() { return provide_calls.load(std::memory_order_relaxed); }
}
