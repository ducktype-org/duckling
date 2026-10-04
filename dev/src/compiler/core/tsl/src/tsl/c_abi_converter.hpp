/**
 * @file c_abi_converter.hpp
 *
 * @brief Query that converts Duckling field types into the minimal
 * `abi::types::AbiType` representation used by
 * `abi::layout::computeCLayout`. Rejects every type that has no
 * representation in the C ABI.
 *
 * The query does not emit diagnostics: it returns a `CAbiConversionResult`
 * holding either the converted type or a human-readable rejection reason.
 * The caller (typically the class-layout helper) is responsible for
 * reporting diagnostics with the proper field name and source position.
 */
#pragma once

#include <abi/type_system/type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <expected>
#include <string>

namespace compiler::tsl {

	/**
	 * @brief Outcome of attempting to map a Duckling field type to its
	 * C-ABI representation: either the converted type or a short rejection
	 * reason suitable for a diagnostic.
	 */
	using CAbiConversionResult = std::expected<abi::types::AbiType, std::string>;

	/**
	 * @brief Converts a Duckling field type to its C-ABI representation.
	 * Returns `Failed` on query cycles (e.g. mutually recursive value-type
	 * classes).
	 */
	DECLARE_QUERY(
		QueryCAbiTypeOf, tsh::SymbolType<>, CRef<query::QResult<CAbiConversionResult>>, ({})
	)
}
