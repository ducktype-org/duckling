/**
 * @file symbols.hpp
 * @brief This file defines Queries responsible for creation of Symbols and operations on them.
 */
#pragma once

#include "generated_symbol_data.hpp"

#include <ctv/ctv.hpp>
#include <frontend/pst_parser/generic_query_key.hpp>
#include <helios/symbols/symbol_id.hpp>
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
	 *
	 * \query_thread_safe_if_cache_and_struct
	 */
	DECLARE_QUERY(QuerySymbolOfSTMT, pst::GenericPSTQueryKey<>, query::QResult<SymID>, ({}));

	/**
	 * @brief Calculates a value of a constant. Returns a CTV containing the result value.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryConstValueOf, SymID, query::QResult<ctv::CompileTimeValue>, ({}));


	using QuerySpecifiersOfSymbol_Result = std::vector<pst::AccessLocked<pst::StmtSpecifier>>;

	/**
	 * @brief Query stmt specifiers associated with given symbol in HELIOS
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QuerySpecifiersOfSymbol,
		SymID,
		CRef<QuerySpecifiersOfSymbol_Result>,
		({ .uses_qresult = false })
	);

	namespace defgen {
		struct KeyFor_QueryGeneratedSymbol {
			base::StrID                name;
			GeneratedSymbolDataVariant generated_symbol_data;

			[[nodiscard]]
			base::Bit256 queryUnstablePerfectHash() const;
		};

		/**
		 * @brief Query a compiler-generated symbol ID, given its identifying parameters.
		 *
		 * @note This query also acts as a cache of SymIDs for generated symbols, so that the same
		 * SymIDs are returned for the same parameters.
		 *
		 * \query_thread_safe_if_cache_and_struct
		 */
		DECLARE_QUERY(
			QueryGeneratedSymbol, KeyFor_QueryGeneratedSymbol, SymID, ({ .uses_qresult = false })
		);
	}

	/**
	 * @brief Symbols used by a function or global symbol, split by kind so callers can pick the
	 * ones they care about: `used_functions` are the functions it references (e.g. call targets),
	 * `used_globals` are the global variables/constants it reads or writes.
	 * @note Shared by both the direct (@ref QueryDirectUsedSymbols) and transitive (@ref
	 * QueryTransitiveUsedSymbols) queries.
	 */
	struct UsedSymbols final {
		std::vector<SymID> used_functions;
		std::vector<SymID> used_globals;
	};

	/**
	 * @brief Query all symbols used directly by a function or global symbol: the functions it calls
	 * and the global variables/constants it references. See @ref UsedSymbols.
	 * @note Works only for SymID-s that represent functions (both PST and generated).
	 * The result doesn't include the queried symbol.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryDirectUsedSymbols, SymID, CRef<query::QResult<UsedSymbols>>, ({}));

	/**
	 * @brief Query all symbols transitively used by a function or global symbol: every function
	 * reachable through the call graph and every global variable/constant referenced by any of
	 * those functions. See @ref UsedSymbols. The result doesn't include the queried symbol.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryTransitiveUsedSymbols, SymID, CRef<query::QResult<UsedSymbols>>, ({}));
}
