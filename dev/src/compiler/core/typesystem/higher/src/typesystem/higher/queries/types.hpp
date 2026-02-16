/**
 * This file contains the queries and functions that create types.
 * Some of those functions are queries and some are simple getters,
 * the division depends mostly on whether we want a query cache or not.
 */

#pragma once

#include "../symbol_type.hpp"
#include "../types.hpp"

#include <base/collections/maps.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <query_framework/query_int.hpp>

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
	IntegralAbstractType getIntegralType(
		query::Context& ctx, u64 size, IntegralAbstractType::Signedness signedness
	);


	/**
	 * @brief Simple getter to create and get floating point types.
	 */
	FloatAbstractType getFloatType(query::Context& ctx, u64 size);

	/**
	 * @brief Simple getter to create and get raw pointer types.
	 */
	RawPointerAbstractType getRawPointerType(bool mutable_pointer);

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
	 * @brief Simple getter to create and get module type.
	 */
	ModuleAbstractType getModuleType();

	/**
	 * @brief Simple getter to create and get import type.
	 */
	ImportAbstractType getImportType();


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
		base::Bit256 queryUnstablePerfectHash() const {
			hashing::DefaultHashAlgorithm hasher{};
			// Note that tuple components are ordered.
			for (const auto& component: components) addToHash(hasher, component);
			return hasher.finalize();
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
		base::Bit256 queryUnstablePerfectHash() const {
			// Note that a variant's component types are *not* ordered, so we have to order them.
			// Let's just hash the components, order them, and then hash the ordered list of hashes.
			std::vector<base::Bit256> hashes{};
			hashes.reserve(underlying_types.size());
			for (const auto& underlying_type: underlying_types)
				hashes.push_back(underlying_type.queryUnstablePerfectHash());
			std::sort(hashes.begin(), hashes.end());

			hashing::DefaultHashAlgorithm hasher{};
			for (const auto& hash: hashes) addToHash(hasher, hash);
			return hasher.finalize();
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
		base::Bit256 queryUnstablePerfectHash() const {
			hashing::DefaultHashAlgorithm hasher{};
			for (const auto& param_type: parameter_types) addToHash(hasher, param_type);
			addToHash(hasher, result_type);
			addToHash(hasher, pure);
			addToHash(hasher, free);
			return hasher.finalize();
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


}
