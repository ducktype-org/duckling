#include <query_framework/query_impl.hpp>

#include "implicit_coercibility.hpp"

#include <set>

namespace ts {
	struct ImplementationOf_QueryImplicitCoercibilityOnInfo:
		  query::QueryImplementation<QueryImplicitCoercibilityOnInfo, bool> {
		inline static base::Map<QKey, query::AddACD<QResult>> cache;

		static auto provide(Context& context, const QKey key) -> PResult {
			return getImplicitConversionsFrom(key.source, context).contains(key.target)
			    || getImplicitConstructorsOf(key.target, context).contains(key.source)
			    || key.source.isInfoImplicitlyCoercible(key.target, context);
		}

		static auto load(const QKey key) -> LoadResult { return cache.atMaybeCopy(key); }

		static auto store(const QKey key, const PResult res, const query::ACD acd) -> QResult {
			cache.put(key, { res, acd });
			return res;
		}

	private:
		static std::set<TypeInfo> getImplicitConversionsFrom(
			[[maybe_unused]] TypeInfo source, [[maybe_unused]] Context& context
		) {
			// @TODO: Add implementation when query for extracting implicit
			// conversion operators for types appears.
			return {};
		}

		static std::set<TypeInfo> getImplicitConstructorsOf(
			[[maybe_unused]] TypeInfo target, [[maybe_unused]] Context& context
		) {
			// @TODO: Add implementation when query for extracting implicit
			// single-argument constructors for types appears.
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(
		ImplementationOf_QueryImplicitCoercibilityOnInfo, "QueryImplicitCoercibilityOnInfo"
	);

	struct ImplementationOf_QueryImplicitCoercibilityOnDesc:
		  query::QueryImplementation<QueryImplicitCoercibilityOnDesc, bool> {
		inline static base::Map<QKey, query::AddACD<QResult>> cache;

		static auto provide(Context& context, const QKey& key) -> PResult {
			return context.query<QueryImplicitCoercibilityOnInfo>(
					   KeyFor_QueryImplicitCoercibilityOnInfo(
						   key.source.getType(), key.target.getType()
					   )
				   )
			    && key.source.getValueCategory().contains(key.target.getValueCategory());
		}

		static auto load(const QKey& key) -> LoadResult {
			if (cache.contains(key)) return cache.at(key);
			return {};
		}

		static auto store(const QKey& key, const PResult res, const query::ACD acd) -> QResult {
			cache.put(key, { res, acd });
			return res;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(
		ImplementationOf_QueryImplicitCoercibilityOnDesc, "QueryImplicitCoercibilityOnDesc"
	);
}
