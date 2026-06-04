/**
 * @file c_abi_converter.hpp
 *
 * @brief Query that converts Duckling field types into the minimal
 * `abi::type_system::AbiType` representation used by
 * `abi::layout::computeCLayout`. Rejects every type that has no
 * representation in the C ABI.
 *
 * The query does not emit diagnostics: it returns a `CAbiConversionResult`
 * with an `Optional` and a human-readable reason. The caller (typically the
 * class-layout helper) is responsible for reporting diagnostics with the
 * proper field name and source position.
 *
 * Being a query, the result is cached and cycle detection is enabled
 * (`uses_qresult=true, allow_cycles=true`).
 */
#pragma once

#include <abi/type_system/type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <string>

namespace compiler::tsl {

	/**
	 * @brief Outcome of attempting to map a Duckling field type to its
	 * C-ABI representation. On success `abi_type` holds the converted type
	 * and `reason` is empty; on failure `abi_type` is empty and `reason`
	 * carries a short explanation suitable for a diagnostic.
	 */
	struct CAbiConversionResult final {
		base::Optional<abi::type_system::AbiType> abi_type;
		std::string                               reason;
	};

	/**
	 * @brief Cached query that converts a Duckling field type to its C-ABI
	 * representation. Returns `Failed` on query cycles (e.g. mutually
	 * recursive value-type classes).
	 */
	DECLARE_QUERY(
		QueryCAbiTypeOf, tsh::SymbolType<>, CRef<query::QResult<CAbiConversionResult>>, ({})
	)
}
