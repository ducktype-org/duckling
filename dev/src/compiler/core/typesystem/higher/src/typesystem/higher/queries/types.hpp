/**
 * This file contains the queries and functions that create types.
 * Some of those functions are queries and some are simple getters,
 * the division depends mostly on whether we want a query cache or not.
 */

#pragma once

#include "../symbol_type.hpp"
#include "../types.hpp"

#include <base/collections/maps.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/utils/simple_keys.hpp>

namespace compiler::tsh {
	/**
	 * @brief Simple getter to create and get the Unit type.
	 */
	UnitAbstractType getUnitType();

	/**
	 * @brief Simple getter to create and get the Void type.
	 */
	VoidAbstractType getVoidType();

	/**
	 * @brief Simple getter to create and get the Byte type.
	 */
	ByteAbstractType getByteType();

	/**
	 * @brief Simple getter to create and get the Bool type.
	 */
	BoolAbstractType getBoolType();

	/**
	 * @brief Simple getter to create and get the Char type.
	 */
	CharAbstractType getCharType();

	/**
	 * @brief Simple getter to create and get integral types.
	 */
	IntegralAbstractType getIntegralType(query::Context& ctx, u64 size, IntegralAbstractType::Signedness signedness);


	/**
	 * @brief Simple getter to create and get floating point types.
	 */
	FloatAbstractType getFloatType(query::Context& ctx, u64 size);

	/**
	 * @brief Simple getter to create and get raw pointer types.
	 */
	RawPointerAbstractType getRawPointerType(query::Context& ctx, bool mutable_pointer);

	/**
	 * @brief Simple getter to create and get string type.
	 */
	StringAbstractType getStringType();

	/**
	 * @brief Simple getter to create and get namespace type.
	 */
	NamespaceAbstractType getNamespaceType();

	/**
	 * @brief Simple getter to create and get meta type.
	 */
	MetaAbstractType getMetaType();



	/**
	 * @brief Query to get a typed Pointer type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryPointerType, SymbolType<>, PointerAbstractType, ({ .uses_qresult = false }))





	/**
	 * @brief Query to get the DynamicArray type.
	 * The AbstractType of the elements of the array is given as a key.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDynamicArrayType, SymbolType<>, DynamicArrayAbstractType, ({ .uses_qresult = false })
	)

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

	/**
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryTupleType, KeyFor_QueryTupleType, TupleAbstractType, ({ .uses_qresult = false })
	)

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

	/**
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryVariantType, KeyFor_QueryVariantType, VariantAbstractType, ({ .uses_qresult = false })
	)

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
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryFunctionType,
		KeyFor_QueryFunctionType,
		FunctionAbstractType,
		({ .uses_qresult = false })
	)

	/**
	 * @brief Query to get the Class type.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryClassType, compiler::helios::SymID, ClassAbstractType, ({ .uses_qresult = false })
	)




	/**
	 * @brief Query to get the Module type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryModuleType, query::EmptyKey, ModuleAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the Import type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryImportType, query::EmptyKey, ImportAbstractType, ({ .uses_qresult = false }))
}
