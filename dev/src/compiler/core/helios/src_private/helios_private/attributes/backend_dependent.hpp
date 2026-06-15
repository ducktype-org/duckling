#pragma once

#include <helios/hout/hout.hpp>

#include <base/types/ok_bad.hpp>

namespace compiler::helios {
    /**
     * @brief Given the symbol of a function declaration with `BackendDependent` attribute from PST,
     * return the symbols of the corresponding function implementations with `DVMOnlyImpl` and `NativeOnlyImpl`
     * or throw a query error.
     */
	[[nodiscard]] std::vector<SymID> getBackendDependentImplementations(query::Context& ctx, SymID sym_id);

    /**
     * @brief Verify the `DVMOnlyImpl` and `NativeOnlyImpl` attributes usage on function declarations.
     * Throws a query error if the usage is incorrect.
     */
	void verifyBackendImplAttrUsage(
		query::Context& ctx, const HOUTFunctionDeclaration& fun_decl
	);

    /**
     * @brief Verify the `BackendDependent` attribute usage on function declaration.
     * Throws a query error if the usage is incorrect.
     */
	void verifyBackendDependentAttrUsage(
		query::Context& ctx, const HOUTFunctionDeclaration& fun_decl
	);
}
