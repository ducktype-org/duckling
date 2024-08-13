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
		static auto provide(Context& ctx, const QKey key) -> PResult {
			auto& tuple_components = key.value->getComponents();
			return sumTypeVectorSizes(tuple_components, ctx);
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
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
		return max;
	}

	struct IMPLEMENT_QUERY(QuerySizeOfVariant, usize) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			auto& variant_components = key.value->getUnderlyingTypes();
			// 1 byte is for information which type is it. Maybe dynamic size in the future.
			return maxTypeVectorSizes(variant_components, ctx) + BYTE_SIZE;
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySizeOfVariant)

	struct IMPLEMENT_QUERY(QuerySizeOfClass, usize) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			const auto& interface = key.value->getInterface(ctx);
			usize       result    = 0;
			for (auto& [k, elems]: interface.getElements()) {
				for (auto& elem: elems)
					if (elem.isField()) result += elem.getResultType().getSize(ctx);
			}
			return result;
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySizeOfClass)
}
