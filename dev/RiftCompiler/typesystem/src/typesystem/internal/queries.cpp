#include "queries.hpp"

#include <query_framework/query_impl.hpp>

#include "type_info_impl.hpp"

namespace ts::internal {

	/**
	 * @brief Gets the sum of the sizes of the types in a vector.
	 * @param types Vector of types to aggregate over.
	 * @param ctx The Query Context necessary to deduce composite type sizes.
	 * @return The total size od the types in the vector.
	 */
	usize sumTypeVectorSizes(const std::vector<ComponentType>& types, query::Context& ctx) {
		usize sum = 0;
		for (const auto& type: types) sum += type.type.getSize(ctx);
		return sum;
	}

	struct IMPLEMENT_QUERY(QuerySizeOfTuple, usize) {
		static inline base::Map<QKey, query::CacheEntry<QResult>> cache;

		static auto provide(Context& ctx, const QKey key) -> PResult {
			auto& tuple_components = key.value->getComponents();
			return sumTypeVectorSizes(tuple_components, ctx);
		}

		static auto store(const QKey key, const PResult p_res, const query::ACD acd) -> QResult {
			cache.emplace(key, query::CacheEntry<QResult>{ p_res, acd });
			return p_res;
		}

		static auto load(const QKey key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySizeOfTuple)

	/**
	 * @brief Gets the maximum size of a type in a vector.
	 * @param types Vector of types to aggregate over.
	 * @param ctx The Query Context necessary to deduce composite type sizes.
	 * @return The maximum size of a type in the vector.
	 */
	usize maxTypeVectorSizes(const std::vector<TypeInfo>& types, query::Context& ctx) {
		usize max = 0;
		for (const auto& type: types) max = std::max(max, type.getSize(ctx));
		// 1 byte is for information which type is it. Maybe dynamic size in the future.
		return BYTE_SIZE + max;
	}

	struct IMPLEMENT_QUERY(QuerySizeOfVariant, usize) {
		static inline base::Map<QKey, query::CacheEntry<QResult>> cache;

		static auto provide(Context& ctx, const QKey key) -> PResult {
			auto& variant_components = key.value->getUnderlyingTypes();
			return maxTypeVectorSizes(variant_components, ctx);
		}

		static auto store(const QKey key, const PResult p_res, const query::ACD acd) -> QResult {
			cache.emplace(key, query::CacheEntry<QResult>{ p_res, acd });
			return p_res;
		}

		static auto load(const QKey key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySizeOfVariant)

	struct IMPLEMENT_QUERY(QuerySizeOfClass, usize) {
		static inline base::Map<QKey, query::CacheEntry<QResult>> cache;

		static auto provide(Context& ctx, const QKey key) -> PResult {
			auto& interface = key.value->getInterface(ctx);
			usize result    = 0;
			for (auto& [k, elems]: interface.getElements()) {
				for (auto& elem: elems)
					if (elem.isField()) result += elem.getResultType().getSize(ctx);
			}
			return result;
		}

		static auto store(const QKey key, const PResult p_res, const query::ACD acd) -> QResult {
			cache.emplace(key, query::CacheEntry<QResult>{ p_res, acd });
			return p_res;
		}

		static auto load(const QKey key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySizeOfClass)
}
