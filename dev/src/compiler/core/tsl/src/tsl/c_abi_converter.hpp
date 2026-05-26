/**
 * @file c_abi_converter.hpp
 *
 * @brief Converts Duckling field types into the minimal `abi::type_system::AbiType`
 * representation used by `abi::layout::computeCLayout`. Rejects every type
 * that has no representation in the C ABI.
 *
 * The converter does not emit diagnostics: it returns an `Optional` together
 * with a human-readable reason. The caller (typically the class-layout
 * helper) is responsible for reporting diagnostics with the proper field
 * name and source position.
 */
#pragma once

#include <abi/type_system/type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context/context_fd.hpp>

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
	 * @brief Tries to map a Duckling field type to its C-ABI representation.
	 *
	 * Accepts integers of widths 8/16/32/64, the Byte type, raw pointers,
	 * fixed-size arrays of accepted types and nested classes that themselves
	 * have `CAbi`. Everything else (references, typed pointers, floats,
	 * tuples, variants, dynamic arrays, functions, units, ...) is rejected.
	 */
	CAbiConversionResult tryConvertToCAbiType(
		compiler::tsh::SymbolType<> field_type, query::Context& ctx
	);

}
