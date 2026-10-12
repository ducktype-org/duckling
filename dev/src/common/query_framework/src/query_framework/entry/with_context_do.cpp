// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "with_context_do.hpp"

#include "query_entry_point.hpp"

#include <concurrent/base/locks/assert_lock.hpp>

#include <base/extend_cpp/defer.hpp>

#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace query::utils {
	namespace {
		struct KeyFor_DoWithContext final {
			std::function<std::any(query::Context&)> value;
			usize                                    id;

			static inline std::atomic<usize> next_id = 0;

			KeyFor_DoWithContext(std::function<std::any(query::Context&)> value):
				  value(std::move(value)),
				  id(next_id.fetch_add(1)) {}

			[[nodiscard]]
			u64 queryUnstablePerfectHash() const {
				return id;
			}
		};

		/**
		 * \parallel thread-safe as long as the passed function is thread-safe
		 */
		DECLARE_QUERY(DoWithContext, KeyFor_DoWithContext, std::any, ({ .uses_qresult = false }))

		struct IMPLEMENT_QUERY(DoWithContext, std::any) {
			static auto provide(Context& ctx, const QKey& key) -> PResult { return key.value(ctx); }

			QUERY_AUTO_CACHE_COPY
		};

		QUERY_IMPLEMENTATION_BOILERPLATE(DoWithContext)
	}

	std::any withContextCompute(std::function<std::any(query::Context&)> action) {
		static concurrent::AssertLock lock;
		lock.lock();
		defer(lock.unlock());

		return query::entryPoint<DoWithContext>(std::move(action));
	}

	void withContextDo(std::function<void(query::Context&)> action) {
		static concurrent::AssertLock lock;
		lock.lock();
		defer(lock.unlock());

		query::entryPoint<DoWithContext>({ [&](query::Context& ctx) -> std::any {
			action(ctx);
			return {};
		} });
	}
}
