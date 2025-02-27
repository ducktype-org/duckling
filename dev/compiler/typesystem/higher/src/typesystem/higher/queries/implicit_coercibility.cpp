#include <query_framework/query_impl.hpp>

#include "implicit_coercibility.hpp"

#include <set>

namespace tsh {
	struct IMPLEMENT_QUERY(QueryImplicitCoercibilityOnAbstractType, bool) {
		inline static base::Map<QKey, query::CacheEntry<QResult>> cache;

		static auto provide(Context& context, const QKey key) -> PResult {
			return key.source == key.target
			    || getImplicitConversionsFrom(key.source, context).contains(key.target)
			    || getImplicitConstructorsOf(key.target, context).contains(key.source)
			    || key.source.isImplicitlyCoercible(key.target, context);
		}

		static auto load(const QKey key) -> LoadResult { return cache.atMaybeCopy(key); }

		static auto store(const QKey key, const PResult res, const query::ACD acd) -> QResult {
			cache.put(key, { res, acd });
			return res;
		}

	private:
		static std::set<AbstractType> getImplicitConversionsFrom(
			[[maybe_unused]] AbstractType source, [[maybe_unused]] Context& context
		) {
			// @TODO: Add implementation when query for extracting implicit
			// conversion operators for types appears.
			return {};
		}

		static std::set<AbstractType> getImplicitConstructorsOf(
			[[maybe_unused]] AbstractType target, [[maybe_unused]] Context& context
		) {
			// @TODO: Add implementation when query for extracting implicit
			// single-argument constructors for types appears.
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitCoercibilityOnAbstractType);

	struct IMPLEMENT_QUERY(QueryImplicitCoercibilityOnDesc, bool) {
		inline static base::Map<QKey, query::CacheEntry<QResult>> cache;

		static auto provide(Context& context, const QKey& key) -> PResult {
			bool type_coercibility = context.query<QueryImplicitCoercibilityOnAbstractType>(
				KeyFor_QueryImplicitCoercibilityOnAbstractType(
					key.source.getType(), key.target.getType()
				)
			);
			bool vc_coercibility
				= key.source.getValueCategory().contains(key.target.getValueCategory());
			return type_coercibility && vc_coercibility;
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitCoercibilityOnDesc);
}
