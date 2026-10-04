#pragma once

#include "abstract_type_impl.hpp"

#include <helios/tsh/type_interface.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::tsh {
	/**
	 * @brief A "stupid" key, containing only a pointer value and defining hashing.
	 */
	struct WrappedClassAbstractTypeImplPtr {
		const ClassAbstractTypeImpl* value;
		WrappedClassAbstractTypeImplPtr() = delete;

		WrappedClassAbstractTypeImplPtr(const ClassAbstractTypeImpl* value): value(value) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return value->toAbstractType().as<ClassAbstractType>().queryUnstablePerfectHash();
		}
	};

	/**
	 * TSH-private query to get the interface of a class.
	 *
	 * To access the interface of a class from outside the TSH module, use
	 * `AbstractType::getInterface`
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their interfaces is trivial.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryInterfaceOfClass,
		WrappedClassAbstractTypeImplPtr,
		CRef<query::QResult<TypeInterface>>,
		({})
	)

	/**
	 * @brief A "stupid" key, containing only a pointer value and defining hashing.
	 */
	struct WrappedTupleAbstractTypeImplPtr {
		const TupleAbstractTypeImpl* value;
		WrappedTupleAbstractTypeImplPtr() = delete;

		WrappedTupleAbstractTypeImplPtr(const TupleAbstractTypeImpl* value): value(value) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return value->toAbstractType().queryUnstablePerfectHash();
		}
	};

	/**
	 * TSH-private query to get the interface of a tuple.
	 *
	 * To access the interface of a tuple from outside the TSH module, use
	 * `AbstractType::getInterface`
	 *
	 * @note This query is made for the purpose of caching. Analogous queries for most other
	 * types do not exist, because getting their interfaces is trivial.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryInterfaceOfTuple,
		WrappedTupleAbstractTypeImplPtr,
		CRef<query::QResult<TypeInterface>>,
		({})
	)

	/**
	 * TSH-private query to get the interface of a static array.
	 *
	 * To access the interface of a static array from outside the TSH module, use
	 * `AbstractType::getInterface`
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryInterfaceOfStaticArray,
		StaticArrayAbstractType,
		CRef<query::QResult<TypeInterface>>,
		({})
	)

	/**
	 * TSH-private query to get the interface of a slice.
	 *
	 * To access the interface of a slice from outside the TSH module, use
	 * `AbstractType::getInterface`
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryInterfaceOfSlice, SliceAbstractType, CRef<query::QResult<TypeInterface>>, ({}))
}
