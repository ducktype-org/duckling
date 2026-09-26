#pragma once

#include "lookup_result.hpp"

#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	struct KeyOf_LookupInNamespaceOrModule {
		/**
		 * @brief Symbol of the module or the namespace.
		 */
		SymID symbol;

		/**
		 * @brief Name to lookup
		 */
		base::StrID name;

		/**
		 * @brief Should wildcards be included in lookup
		 */
		bool with_wildcards;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Query result of lookup of single name within the using or import statement.
	 *
	 * \query_thread_safe_if_cache_and_struct
	 */
	DECLARE_QUERY(
		QueryLookupInNamespaceOrModule,
		KeyOf_LookupInNamespaceOrModule,
		CRef<query::QResult<LookupResult>>,
		({})
	);

	struct KeyOf_LookupInUsingOrImport {
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
	 * @brief Query result of lookup of single name within the using or import statement.
	 *
	 * \query_thread_safe_if_cache_and_struct
	 */
	DECLARE_QUERY(
		QueryLookupInUsingImport,
		KeyOf_LookupInUsingOrImport,
		CRef<query::QResult<LookupResult>>,
		({})
	);
}
