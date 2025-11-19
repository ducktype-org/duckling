#include "with_context_do.hpp"

#include "../query_entry_point.hpp"
#include "../query_impl.hpp"

namespace query::utils {
	namespace {
		struct KeyFor_DoWithContext final {
			std::function<std::any(query::Context&)> value;
			usize                                    id;
			static inline usize                      next_id = 0;

			KeyFor_DoWithContext(std::function<std::any(query::Context&)> value):
				  value(std::move(value)),
				  id(next_id++) {}

			[[nodiscard]]
			u64 queryUnstablePerfectHash() const {
				return id;
			}
		};

		DECLARE_QUERY(DoWithContext, KeyFor_DoWithContext, std::any, ({}))

		struct IMPLEMENT_QUERY(DoWithContext, std::any) {
			static auto provide(Context& ctx, const QKey& key) -> PResult { return key.value(ctx); }

			QUERY_AUTO_NO_CACHE
		};

		QUERY_IMPLEMENTATION_BOILERPLATE(DoWithContext)
	}

	std::any withContextCompute(std::function<std::any(query::Context&)> action) {
		return query::entryPoint<DoWithContext>(std::move(action));
	}

	void withContextDo(std::function<void(query::Context&)> action) {
		query::entryPoint<DoWithContext>({ [&](query::Context& ctx) -> std::any {
			action(ctx);
			return {};
		} });
	}
}
