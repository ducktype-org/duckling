#pragma once

#include "../types.hpp"

#include <base/maps.hpp>

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
	 * The boolean key denotes whether the raw pointer points to mutable data.
	 */
	DECLARE_QUERY(QueryRawPointerType, bool, RawPointerInfo)

	/**
	 * @brief Query to get a typed Pointer type.
	 */
	DECLARE_QUERY(QueryPointerType, ComponentType, PointerInfo)

	/**
	 * @brief Key for QueryTupleType.
	 */
	struct KeyFor_QueryTupleType {
		std::vector<ComponentType> components;

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryTupleType&) const
			= default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			static base::Map<KeyFor_QueryTupleType, u64> hashes{};

			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

			u64 result = hashes.size();
			hashes.put(*this, result);
			return result;
		}
	};

	DECLARE_QUERY(QueryTupleType, KeyFor_QueryTupleType, TupleInfo)

	/**
	 * @brief Key for QueryVariantType.
	 */
	struct KeyFor_QueryVariantType {
		std::vector<TypeInfo> underlying_types;

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryVariantType&) const
			= default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			static base::Map<KeyFor_QueryVariantType, u64> hashes{};

			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

			u64 result = hashes.size();
			hashes.put(*this, result);
			return result;
		}
	};

	DECLARE_QUERY(QueryVariantType, KeyFor_QueryVariantType, VariantInfo)

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
		bool pure = false;

		/**
		 * @brief Whether the function type is free or not.
		 *
		 * See documentation of FunctionInfo for details.
		 */
		bool free = false;

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryFunctionType&) const
			= default;

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
	 * @brief Query to get the Class type.
	 */
	DECLARE_QUERY(QueryClassType, compiler::helios::SymID, ClassInfo)

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
