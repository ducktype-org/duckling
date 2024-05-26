#pragma once

#include "../types.hpp"

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
	DECLARE_QUERY(QueryRawPointerType, bool, RawPointerInfo)

	/**
	 * @brief Query to get a typed Pointer type.
	 */
	DECLARE_QUERY(QueryPointerType, ComponentType, PointerInfo)

	/**
	 * @brief Key for QueryFunctionType.
	 */
	struct KeyFor_QueryFunctionType {
		/**
		 * @brief The types of the parameters of the function.
		 */
		std::vector<TypeInfo> parameter_types;

		/**
		 * @brief The result type of the function.
		 */
		TypeInfo result_type;

		/**
		 * @brief Whether the function type is pure or not.
		 *
		 * See documentation of FunctionInfo for details.
		 */
		bool pure;

		/**
		 * @brief Whether the function type is free or not.
		 *
		 * See documentation of FunctionInfo for details.
		 */
		bool free;

		// This spaceship definition is required because TypeInfo has a spaceship definition.
		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryFunctionType&) const
			= default;

		// These constructor definitions are to force giving at least the first two arguments.
		// Initializer lists still work.
		KeyFor_QueryFunctionType() = delete;

		KeyFor_QueryFunctionType(
			std::vector<TypeInfo> parameter_types,
			const TypeInfo        result_type,
			const bool            pure = false,
			const bool            free = false
		):
			  parameter_types(std::move(parameter_types)),
			  result_type(result_type),
			  pure(pure),
			  free(free) {}

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			static base::Map<KeyFor_QueryFunctionType, u64> hashes{};

			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

			u64 result = hashes.size();
			hashes.put(*this, result);
			return result;
		}
	};

	/**
	 * @brief Query to get the Function type.
	 */
	DECLARE_QUERY(QueryFunctionType, KeyFor_QueryFunctionType, FunctionInfo)

	/**
	 * @brief Query to get the Meta type.
	 */
	DECLARE_QUERY(QueryMetaType, query::EmptyKey, MetaInfo)

	/**
	 * @brief Query to get the Namespace type.
	 */
	DECLARE_QUERY(QueryNamespaceType, query::EmptyKey, NamespaceInfo)

	/**
	 * @brief Query to get the Module type.
	 */
	DECLARE_QUERY(QueryModuleType, query::EmptyKey, ModuleInfo)
}
