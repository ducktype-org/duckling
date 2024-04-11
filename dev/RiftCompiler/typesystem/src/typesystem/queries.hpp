#pragma once

#include <query_framework/query_int.hpp>

#include "type_info.hpp"

namespace ts {
	/**
	 * @brief Query to get the Unit type.
	 */
	DECLARE_QUERY(QueryUnitType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Query to get the Void type.
	 */
	DECLARE_QUERY(QueryVoidType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Query to get the Byte type.
	 */
	DECLARE_QUERY(QueryByteType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Query to get the Bool type.
	 */
	DECLARE_QUERY(QueryBoolType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Query to get the Char type.
	 */
	DECLARE_QUERY(QueryCharType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Key for QueryIntegralType.
	 */
	struct KeyFor_QueryIntegralType {
		/**
		 * @brief The size of the Integral type. Pick from { 8, 16, 32, 64, 128 }.
		 */
		usize size;

		/**
		 * @brief Whether the Integral type is signed or not.
		 */
		bool signedness;
	};

	/**
	 * @brief Query to get an Integral type.
	 */
	DECLARE_QUERY(QueryIntegralType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Query to get a Float (floating point) type.
	 */
	DECLARE_QUERY(QueryFloatType, usize, TypeInfo)

	/**
	 * @brief Query to get a RawPointer type.
	 */
	DECLARE_QUERY(QueryRawPointerType, query::EmptyKey, TypeInfo)

	/**
	 * @brief Key for QueryPointerType.
	 */
	struct KeyFor_QueryPointerType {
		/**
		 * @brief The underlying type of the pointer.
		 */
		TypeInfo underlying_type;

		/**
		 * @brief Whether the data under the pointer is mutable or not.
		 */
		bool is_mutable;
	};

	/**
	 * @brief Query to get a (typed) Pointer type.
	 */
	DECLARE_QUERY(QueryPointerType, KeyFor_QueryPointerType, TypeInfo)
}
