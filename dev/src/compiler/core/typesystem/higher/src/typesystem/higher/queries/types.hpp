#pragma once

#include "../symbol_type.hpp"
#include "../types.hpp"

#include <base/collections/maps.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/simple_keys.hpp>

namespace tsh {
	/**
	 * @brief Query to get the Unit type.
	 */
	DECLARE_QUERY(QueryUnitType, query::EmptyKey, UnitAbstractType, ({}))

	/**
	 * @brief Query to get the Void type.
	 */
	DECLARE_QUERY(QueryVoidType, query::EmptyKey, VoidAbstractType, ({}))

	/**
	 * @brief Query to get the Byte type.
	 */
	DECLARE_QUERY(QueryByteType, query::EmptyKey, ByteAbstractType, ({}))

	/**
	 * @brief Query to get the Bool type.
	 */
	DECLARE_QUERY(QueryBoolType, query::EmptyKey, BoolAbstractType, ({}))

	/**
	 * @brief Query to get the Char type.
	 */
	DECLARE_QUERY(QueryCharType, query::EmptyKey, CharAbstractType, ({}))

	/**
	 * @brief Key for QueryIntegralType.
	 */
	struct KeyFor_QueryIntegralType final {
		/**
		 * @brief The size of the Integral type. Pick from { 8, 16, 32, 64, 128 }.
		 */
		usize size;

		/**
		 * @brief Whether the Integral type is signed or not.
		 */
		IntegralAbstractType::Signedness signedness;

		// These constructor definitions are to force giving at least the first argument.
		KeyFor_QueryIntegralType() = delete;

		KeyFor_QueryIntegralType(
			const usize                            size,
			const IntegralAbstractType::Signedness signedness
			= IntegralAbstractType::Signedness::Signed
		):
			  size(size),
			  signedness(signedness) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return size + (signedness == IntegralAbstractType::Signedness::Signed);
		}
	};

	/**
	 * @brief Query to get an Integral type.
	 */
	DECLARE_QUERY(QueryIntegralType, KeyFor_QueryIntegralType, IntegralAbstractType, ({}))

	/**
	 * @brief Query to get a Float (floating point) type.
	 */
	DECLARE_QUERY(QueryFloatType, query::U64Key, FloatAbstractType, ({}))


	// @TODO PR: fix u64/bool queries

	/**
	 * @brief Query to get a RawPointer type.
	 * The boolean key denotes whether the raw pointer points to mutable data.
	 */
	DECLARE_QUERY(QueryRawPointerType, query::BoolKey, RawPointerAbstractType, ({}))

	/**
	 * @brief Query to get a typed Pointer type.
	 */
	DECLARE_QUERY(QueryPointerType, SymbolType<>, PointerAbstractType, ({}))

	/**
	 * @brief Query to get the String type.
	 */
	DECLARE_QUERY(QueryStringType, query::EmptyKey, StringAbstractType, ({}))

	/**
	 * @brief Query to get the DynamicArray type.
	 * The AbstractType of the elements of the array is given as a key.
	 */
	DECLARE_QUERY(QueryDynamicArrayType, SymbolType<>, DynamicArrayAbstractType, ({}))

	/**
	 * @brief Key for QueryTupleType.
	 */
	struct KeyFor_QueryTupleType final {
		std::vector<SymbolType<>> components;

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryTupleType&) const
			= default;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			static base::Map<KeyFor_QueryTupleType, u64> hashes{};

			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

			u64 result = hashes.size();
			hashes.put(*this, result);
			return result;
		}
	};

	DECLARE_QUERY(QueryTupleType, KeyFor_QueryTupleType, TupleAbstractType, ({}))

	/**
	 * @brief Key for QueryVariantType.
	 */
	struct KeyFor_QueryVariantType final {
		std::vector<SymbolType<>> underlying_types;

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryVariantType&) const
			= default;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			static base::Map<KeyFor_QueryVariantType, u64> hashes{};

			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;

			u64 result = hashes.size();
			hashes.put(*this, result);
			return result;
		}
	};

	DECLARE_QUERY(QueryVariantType, KeyFor_QueryVariantType, VariantAbstractType, ({}))

	/**
	 * @brief Key for QueryFunctionType.
	 */
	struct KeyFor_QueryFunctionType final {
		/**
		 * @brief The types of the parameters of the function.
		 */
		std::vector<SymbolType<>> parameter_types;

		/**
		 * @brief The result type of the function.
		 */
		SymbolType<> result_type;

		/**
		 * @brief Whether the function type is pure or not.
		 *
		 * See documentation of FunctionAbstractType for details.
		 */
		bool pure = false;

		/**
		 * @brief Whether the function type is free or not.
		 *
		 * See documentation of FunctionAbstractType for details.
		 */
		bool free = false;

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryFunctionType&) const
			= default;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
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
	DECLARE_QUERY(QueryFunctionType, KeyFor_QueryFunctionType, FunctionAbstractType, ({}))

	/**
	 * @brief Query to get the Class type.
	 */
	DECLARE_QUERY(QueryClassType, compiler::helios::SymID, ClassAbstractType, ({}))

	/**
	 * @brief Query to get the Meta type.
	 */
	DECLARE_QUERY(QueryMetaType, query::EmptyKey, MetaAbstractType, ({}))

	/**
	 * @brief Query to get the Namespace type.
	 */
	DECLARE_QUERY(QueryNamespaceType, query::EmptyKey, NamespaceAbstractType, ({}))

	/**
	 * @brief Query to get the Module type.
	 */
	DECLARE_QUERY(QueryModuleType, query::EmptyKey, ModuleAbstractType, ({}))

	/**
	 * @brief Query to get the Import type.
	 */
	DECLARE_QUERY(QueryImportType, query::EmptyKey, ImportAbstractType, ({}))
}
