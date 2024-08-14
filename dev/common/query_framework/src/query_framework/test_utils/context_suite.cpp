#include "context_suite.hpp"

#include <query_framework/query_entry_point.hpp>

namespace tester {
	namespace {
		struct KeyFor_DoWithContext {
			std::function<void*(query::Context&)> value;
			usize                                 ID;
			static inline usize                   nextID = 0;

			KeyFor_DoWithContext(std::function<void*(query::Context&)> value):
				  value(std::move(value)),
				  ID(nextID++) {}

			[[nodiscard]]
			base::HashT customPerfectHash() const {
				return base::HashT(ID);
			}
		};

		DECLARE_QUERY(DoWithContext, KeyFor_DoWithContext, void*)

		struct IMPLEMENT_QUERY(DoWithContext, void*) {
			static auto provide(Context& ctx, const QKey& key) -> PResult { return key.value(ctx); }

			QUERY_AUTO_NO_CACHE
		};

		QUERY_IMPLEMENTATION_BOILERPLATE(DoWithContext)
	}

	/**
	 * @brief Execute a function as if it were in the middle of a query, i.e. supplied with a
	 * query::Context.
	 * @param action The function object to execute, applied to a query::Context.
	 * @return The (pointer to the) result of the function.
	 */
	void* ContextSuite::withContext(std::function<void*(query::Context&)> action) {
		return query::entryPoint<DoWithContext>(action);
	}
}
