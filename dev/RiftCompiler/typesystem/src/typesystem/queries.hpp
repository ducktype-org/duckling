#pragma once

#include <query_framework/query_int.hpp>

#include "base/perfect_hash.hpp"
#include "type_info.hpp"
#include "types.hpp"

namespace ts {
	/**
	 * @brief Query to get the Unit type.
	 */
	DECLARE_QUERY(QueryUnitType, query::EmptyKey, UnitInfo)

	/**
	 * @brief Query to get the Void type.
	 */
	DECLARE_QUERY(QueryVoidType, query::EmptyKey, VoidInfo)

	/**
	 * @brief Query to get the Byte type.
	 */
	DECLARE_QUERY(QueryByteType, query::EmptyKey, ByteInfo)

	/**
	 * @brief Query to get the Bool type.
	 */
	DECLARE_QUERY(QueryBoolType, query::EmptyKey, BoolInfo)

	/**
	 * @brief Query to get the Char type.
	 */
	DECLARE_QUERY(QueryCharType, query::EmptyKey, CharInfo)

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
		bool signedness{ true };

		// These constructor definitions are to force giving at least the first argument.
		KeyFor_QueryIntegralType() = delete;

		KeyFor_QueryIntegralType(const usize size, const bool signedness = true):
			  size(size),
			  signedness(signedness) {}


		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return size + signedness;
		}
	};

	/**
	 * @brief Query to get an Integral type.
	 */
	DECLARE_QUERY(QueryIntegralType, KeyFor_QueryIntegralType, IntegralInfo)

	/**
	 * @brief Query to get a Float (floating point) type.
	 */
	DECLARE_QUERY(QueryFloatType, usize, FloatInfo)

	/**
	 * @brief Query to get a RawPointer type.
	 */
	DECLARE_QUERY(QueryRawPointerType, query::EmptyKey, RawPointerInfo)

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
		bool is_mutable{ false };

		// This spaceship definition is required because TypeInfo has a spaceship definition.
		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryPointerType& other) const
			= default;

		// These constructor definitions are to force giving at least the first argument.
		// Initializer lists still work.
		KeyFor_QueryPointerType() = delete;

		KeyFor_QueryPointerType(const TypeInfo underlying_type, const bool is_mutable = false):
			  underlying_type(underlying_type),
			  is_mutable(is_mutable) {}

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return reinterpret_cast<std::size_t>(underlying_type.getPimpl()) + is_mutable;
		}
	};

	/**
	 * @brief Query to get a (typed) Pointer type.
	 */
	DECLARE_QUERY(QueryPointerType, KeyFor_QueryPointerType, PointerInfo)

	/**
	 * @brief Query to get the Meta type.
	 */
	DECLARE_QUERY(QueryMetaType, query::EmptyKey, MetaInfo)
}


