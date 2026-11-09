// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>

#include <base/types/ok_bad.hpp>

#include <string>

namespace compiler::helios {

	struct FunctionDeclarationCompareParams final {
		/**
		 * @brief For the comparison also compare the parameter names.
		 */
		bool compare_parameter_names = false;

		/**
		 * @brief For the comparison also compare the initial values.
		 * @warning Not supported, as comparing the HOUT expressions is not supported.
		 */
		bool compare_initial_values = false;

		/**
		 * @brief When true, logs a diagnostic error for each mismatch found.
		 * When false, the function only returns whether the declarations match.
		 */
		bool log_error_on_mismatch = true;

		/**
		 * @brief Explains why the two declarations must match.
		 * Included in every error message. Empty string is used when not set.
		 */
		base::Optional<std::string> reason{};
	};

	/**
	 * @brief Compares two function declarations for signature compatibility. Always compares return
	 * type and parameter types; optionally also compares parameter names and initial-value
	 * presence. When @p params.log_error_on_mismatch is true, logs a diagnostic error with source
	 * positions for each mismatch found.
	 *
	 * @return true if the declarations are equivalent under the given params.
	 */
	[[nodiscard]]
	bool compareFunctionDeclarations(
		query::Context&                  ctx,
		const HOUTFunctionDeclaration&   a,
		const HOUTFunctionDeclaration&   b,
		FunctionDeclarationCompareParams params
	);

	/**
	 * @brief Reports diagnostics if the function declaration uses initial value expression.
	 * Used when we want to enforce no initial values, for example with some attributes.
	 *
	 * @param reason Reason is displayed as part of error messaged.
	 * @return base::OkBad Ok if no initial values in the function declarations, Bad if there are
	 * and error is logged to context.
	 */
	[[nodiscard]]
	base::OkBad ensureFunctionDeclarationNoInitialValue(
		query::Context& ctx, const HOUTFunctionDeclaration& fun, const std::string& reason
	);
}
