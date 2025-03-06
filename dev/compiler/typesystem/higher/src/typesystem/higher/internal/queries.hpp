#pragma once

#include <query_framework/query_int.hpp>

#include "../type_interface.hpp"

namespace tsh::internal {
	class ClassAbstractTypeImpl;

	/**
	 * @brief A "stupid" key, containing only a pointer value and defining comparison and hashing.
	 */
	struct WrappedClassAbstractTypeImplPtr {
		const ClassAbstractTypeImpl* value;
		WrappedClassAbstractTypeImplPtr() = delete;

		WrappedClassAbstractTypeImplPtr(const ClassAbstractTypeImpl* value): value(value) {}

		auto operator<=>(const WrappedClassAbstractTypeImplPtr& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return base::HashT(value);
		}
	};

	/**
	 * TS-internal query to get the interface of a class.
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their interfaces is trivial.
	 */
	DECLARE_QUERY(QueryInterfaceOfClass, WrappedClassAbstractTypeImplPtr, CRef<TypeInterface>)
}
