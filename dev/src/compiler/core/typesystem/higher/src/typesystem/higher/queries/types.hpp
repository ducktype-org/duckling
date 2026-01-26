#pragma once

#include "../symbol_type.hpp"
#include "../types.hpp"

#include <base/collections/maps.hpp>

#include <query_framework/query_int.hpp>
#include <query_frameworkutils/simple_keys.hpp>

namespace compiler::tsh {
	/**
	 * @brief Query to get the Unit type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryUnitType, query::EmptyKey, UnitAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the Void type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryVoidType, query::EmptyKey, VoidAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the Byte type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryByteType, query::EmptyKey, ByteAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the Bool type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryBoolType, query::EmptyKey, BoolAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the Char type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryCharType, query::EmptyKey, CharAbstractType, ({ .uses_qresult = false }))

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
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(
		QueryIntegralType,
		KeyFor_QueryIntegralType,
		IntegralAbstractType,
		({ .uses_qresult = false })
	)

	/**
	 * @brief Query to get a Float (floating point) type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryFloatType, query::U64Key, FloatAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get a RawPointer type.
	 * The boolean key denotes whether the raw pointer points to mutable data.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(
		QueryRawPointerType, query::BoolKey, RawPointerAbstractType, ({ .uses_qresult = false })
	)

	/**
	 * @brief Query to get a typed Pointer type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryPointerType, SymbolType<>, PointerAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the String type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryStringType, query::EmptyKey, StringAbstractType, ({ .uses_qresult = false }))

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
	 * @brief Query to get the Meta type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(QueryMetaType, query::EmptyKey, MetaAbstractType, ({ .uses_qresult = false }))

	/**
	 * @brief Query to get the Namespace type.
	 *
	 * \query_thread_safe
	 */
	DECLARE_QUERY(
		QueryNamespaceType, query::EmptyKey, NamespaceAbstractType, ({ .uses_qresult = false })
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
