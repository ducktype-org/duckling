/**
 * @file symbols.hpp
 * @brief This file defines Queries responsible for creation of Symbols and operations on them.
 */
#pragma once

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/lookup_utils/lookup_result.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/generic_query_key.hpp>
#include <query_framework/query_int.hpp>
#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <base/string_id.hpp>

namespace compiler::helios {


	/**
	 * @brief Query symbols associated with given element in PST
	 */
	DECLARE_QUERY(QuerySymbolOfSTMT, pst::GenericPSTQueryKey<>, SymID);

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
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_LookupInSymbol&) const = default;
	};

	/**
	 * @brief Query result of lookup of single name within the symbol.
	 * It essentially implements "symbol.name" operation.
	 */
	DECLARE_QUERY(QueryLookupInSymbol, KeyOf_LookupInSymbol, CRef<LookupResult>);

	using QueryDealias_Result = errors::HResult<SymbolList, errors::Failed>;
	/**
	 * A query that returns an "absolute path" to the symbol without aliases.
	 */
	DECLARE_QUERY(QueryDealias, SymID, CRef<QueryDealias_Result>);

	using PotentialParsingErrors = std::
		variant<errors::SymbolNotFound, errors::Ambiguity, errors::InvalidExpr, errors::Failed>;
	/**
	 * Calculates a value of a constant.
	 */
	DECLARE_QUERY(QueryConstValueOf, SymID, errors::HResult<i64 COMMA errors::Failed>);

	namespace builtin {
		/**
		 * Lookup a global builtin symbol by name.
		 * @note Non-global builtins will likely exist, for example: `i64.max`.
		 */
		LookupResult lookupGlobalBuiltins(query::Context&, base::StrID name);
	}
}
