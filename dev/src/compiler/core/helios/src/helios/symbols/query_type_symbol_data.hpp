#pragma once

#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <string_id/string_id.hpp>

namespace compiler::helios {
	struct ClassMemberSpecifiersResult {
		base::Optional<tsh::MemberVisibility> visibility_opt;
		bool                                  is_static;
	};

	ClassMemberSpecifiersResult getClassMemberSpecifiers(query::Context& ctx, SymID sym);

	/**
	 * @brief Struct returned by the `QueryClassSymbolData` query.
	 */
	struct ClassSymbolData {
		/**
		 * @brief Name of the class in the source code.
		 */
		base::StrID name;

		/**
		 * @brief Type interface of the class.
		 */
		tsh::TypeInterface declared_interface;

		// =============== Precomputed type properties ===============

		bool is_trivially_destructible;
		bool is_default_constructible;
		bool is_trivially_zero_initializable;
		bool is_copyable;
		bool is_trivially_copyable;
		bool carries_information;

		// =============== Currently unused ===============
		/**
		 * @brief Class's base class.
		 */
		base::Optional<tsh::AbstractType> base;
		/**
		 * @brief Class's implemented interfaces.
		 */
		std::vector<tsh::AbstractType> implements;
	};

	using QueryClassSymbolData_Result = query::QResult<ClassSymbolData>;

	/**
	 * @brief Query all the information about a class definition.
	 * Panics if the given `SymID` is not a class.
	 * More information on `ClassSymbolData` in its definition.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryClassSymbolData, SymID, CRef<QueryClassSymbolData_Result>, ({}))

	/**
	 * @brief Struct returned by the `QueryTupleTypeData` query.
	 */
	struct TupleTypeData {
		/**
		 * @briefTuple's generated fields.
		 */
		std::vector<SymID> members;
	};

	using QueryTupleTypeData_Result = query::QResult<TupleTypeData>;

	/**
	 * @brief Query all the information about a class definition.
	 * Panics if the given `SymID` is not a class.
	 * More information on `ClassSymbolData` in its definition.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryTupleTypeData, tsh::TupleAbstractType, CRef<QueryTupleTypeData_Result>, ({}))

	struct SliceTypeData {
		SymID ptr;
		SymID len;
	};

	/**
	 * @brief Query field symbols of a slice type.
	 * Panics if the given type is not a slice.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QuerySliceTypeData, tsh::SliceAbstractType, CRef<SliceTypeData>, ({ .uses_qresult = false })
	)
}
