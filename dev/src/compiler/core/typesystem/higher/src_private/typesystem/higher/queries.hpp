#pragma once

#include <typesystem/higher/type_interface.hpp>

#include <query_framework/query_int.hpp>

namespace tsh {
	class ClassAbstractTypeImpl;

	/**
	 * @brief A "stupid" key, containing only a pointer value and defining hashing.
	 */
	struct WrappedClassAbstractTypeImplPtr {
		const ClassAbstractTypeImpl* value;
		WrappedClassAbstractTypeImplPtr() = delete;

		WrappedClassAbstractTypeImplPtr(const ClassAbstractTypeImpl* value): value(value) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return u64(value);
		}
	};

	/**
	 * TSH-private query to get the interface of a class.
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their interfaces is trivial.
	 */
	DECLARE_QUERY(QueryInterfaceOfClass, WrappedClassAbstractTypeImplPtr, CRef<TypeInterface>)
}
