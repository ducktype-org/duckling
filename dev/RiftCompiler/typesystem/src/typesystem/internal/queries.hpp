#pragma once

#include <query_framework/query_int.hpp>

#include "../type_interface.hpp"

namespace ts::internal {
	class TupleInfoImpl;

	/**
	 * @brief A "stupid" key, containing only a pointer value and defining comparison and hashing.
	 */
	struct WrappedTupleInfoImplPtr {
		const ts::internal::TupleInfoImpl* value;
		WrappedTupleInfoImplPtr() = delete;

		WrappedTupleInfoImplPtr(const ts::internal::TupleInfoImpl* value): value(value) {}

		auto operator<=>(const WrappedTupleInfoImplPtr& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return base::HashT(value);
		}
	};

	/**
	 * @brief TS-internal query to get the size of a tuple.
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their sizes is trivial (e.g. 8 for i8).
	 */
	DECLARE_QUERY(QuerySizeOfTuple, WrappedTupleInfoImplPtr, usize)

	class VariantInfoImpl;

	/**
	 * @brief A "stupid" key, containing only a pointer value and defining comparison and hashing.
	 */
	struct WrappedVariantIntoImplPtr {
		const ts::internal::VariantInfoImpl* value;
		WrappedVariantIntoImplPtr() = delete;

		WrappedVariantIntoImplPtr(const ts::internal::VariantInfoImpl* value): value(value) {}

		auto operator<=>(const WrappedVariantIntoImplPtr& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return base::HashT(value);
		}
	};

	/**
	 * @brief TS-internal query to get the size of a variant.
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their sizes is trivial (e.g. 8 for i8).
	 */
	DECLARE_QUERY(QuerySizeOfVariant, WrappedVariantIntoImplPtr, usize)

	class ClassInfoImpl;

	/**
	 * @brief A "stupid" key, containing only a pointer value and defining comparison and hashing.
	 */
	struct WrappedClassInfoImplPtr {
		const ts::internal::ClassInfoImpl* value;
		WrappedClassInfoImplPtr() = delete;

		WrappedClassInfoImplPtr(const ts::internal::ClassInfoImpl* value): value(value) {}

		auto operator<=>(const WrappedClassInfoImplPtr& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return base::HashT(value);
		}
	};

	/**
	 * @brief TS-internal query to get the size of a class.
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their sizes is trivial (e.g. 8 for i8).
	 */
	DECLARE_QUERY(QuerySizeOfClass, WrappedClassInfoImplPtr, usize)

	/**
	 * TS-internal query to get the interface of a class.
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their interfaces is trivial.
	 */
	DECLARE_QUERY(QueryInterfaceOfClass, WrappedClassInfoImplPtr, const TypeInterface&)
}
