/**
 * @file symbols.hpp
 * @brief This file defines Queries responsible for creation of Symbols and operations on them.
 */
#pragma once

#include "generated_symbol_data.hpp"

#include <ctv/ctv.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/helios_errors.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios_private/lookup/lookup_result.hpp>

#include <base/types/bit256.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <string_id/string_id.hpp>

namespace compiler::helios {

	/**
	 * @brief Return all symbols currently stored by HELIOS.
	 * @note: This should be used for tests and debug only,
	 * and never in an actual query.
	 * @return std::vector<SymID>
	 */
	std::vector<SymID> getAllHeliosSymbols();

	/**
	 * @brief Query symbol associated with given element in PST
	 */
	DECLARE_QUERY(
		QuerySymbolOfSTMT,
		pst::GenericPSTQueryKey<>,
		SymID,
		({ .used_hashes = query::UsedHashes::StableHash })
	);

	struct KeyOf_LookupInSymbol {
		/**
		 * @brief Symbol to lookup in
		 */
		SymID symbol;

		/**
		 * @brief Name to lookup
		 */
		base::StrID name;

		/**
		 * @brief Should wildcards be included in lookup
		 */
		bool follow_wildcards;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Query result of lookup of single name within the symbol.
	 * It essentially implements "symbol.name" operation.
	 */
	DECLARE_QUERY(QueryLookupInSymbol, KeyOf_LookupInSymbol, CRef<LookupResult>, ({}));

	using QueryDealias_Result = query::QResult<SymbolList, query::Failed>;

	/**
	 * A query that returns dealiased symbol list of a given alias symbol.
	 */
	DECLARE_QUERY(QueryDealias, SymID, CRef<QueryDealias_Result>, ({}));

	using QueryConstValueOf_Result = query::QResult<ctv::CompileTimeValue, query::Failed>;

	/**
	 * @brief Calculates a value of a constant. Returns a CTV containing the result value.
	 */
	DECLARE_QUERY(QueryConstValueOf, SymID, QueryConstValueOf_Result, ({}));

	namespace builtin {
		/**
		 * Lookup a global builtin symbol by name.
		 * @note Non-global builtins will likely exist, for example: `i64.max`.
		 */
		LookupResult lookupGlobalBuiltins(query::Context&, base::StrID name);
	}

	using QuerySpecifiersOfSymbol_Result = std::vector<pst::AccessLocked<pst::StmtSpecifier>>;

	/**
	 * @brief Query stmt specifiers associated with given symbol in HELIOS
	 */
	DECLARE_QUERY(QuerySpecifiersOfSymbol, SymID, CRef<QuerySpecifiersOfSymbol_Result>, ({}));

	namespace houtgen {
		struct KeyFor_QueryGeneratedSymbol {
			base::StrID         name;
			GeneratedSymbolData generated_symbol_data;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * @brief Query a compiler-generated symbol ID, given its identifying parameters.
		 *
		 * @note This query also acts as a cache of SymIDs for generated symbols, so that the same
		 * SymIDs are returned for the same parameters.
		 */
		DECLARE_QUERY(QueryGeneratedSymbol, KeyFor_QueryGeneratedSymbol, SymID, ({}));
	}

	/**
	 * @brief Query all function dependencies of a function (e.g. for a given function SymID, return
	 * all SymID-s of functions called directly by this one.
	 * @note Works only for SymID-s that actually represent PST-function (i.e. PST symbol).
	 */
	DECLARE_QUERY(QueryDirectFunctionCalls, SymID, CRef<std::vector<SymID>>, ({}));

	/**
	 * @brief Query all function dependencies of a function (e.g. SymID-s of all functions called by
	 * this function or all functions called by the called functions).
	 * @note This query is used to determine all other functions that have to be compiled when
	 * compile time evaluating a function.
	 * @note Works only for SymID-s that actually represent PST-function (i.e. PST symbol).
	 */
	DECLARE_QUERY(QueryTransitiveFunctionCalls, SymID, CRef<std::vector<SymID>>, ({}));
}
