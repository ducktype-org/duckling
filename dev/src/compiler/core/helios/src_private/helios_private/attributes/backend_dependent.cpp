#include "backend_dependent.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/attributes.hpp>
#include <helios_private/hout/hout.hpp>
#include <helios_private/scopes/scopes.hpp>

#include <query_framework/query_errors.hpp>

namespace compiler::helios {

	template<typename Attr>
	SymID searchForSymbolWithAttributeAndName(
		query::Context& ctx, SymID scope_source_sym, base::StrID searched_name
	) {
		auto symbols = ctx.query<QuerySymbolsInScope>(scope(scope_source_sym))->valueOrThrow();
		std::vector<SymID> results;
		for (auto s: symbols)
			if (name(s) == searched_name && hasAttribute<Attr>(s)) results.push_back(s);
		if (results.size() > 1) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(base::strConcat(
				"Multiple symbols with name `",
				searched_name,
				"` and attribute `",
				attrNameStr(Attr{}),
				"` found. Expected at most one."
			)));
			query::throwFailed();
		}
		if (results.empty()) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(base::strConcat(
				"No symbol with name `",
				searched_name,
				"` and attribute `",
				attrNameStr(Attr{}),
				"` found. Expected exactly one."
			)));
			query::throwFailed();
		}
		return results.at(0);
	}

	std::vector<SymID> getBackendDependentImplementations(query::Context& ctx, SymID sym_id) {
		CORE_ASSERT(
			hasAttribute<attributes::BackendDependent>(sym_id), "Called with invalid symbol"
		);

		std::vector<SymID> result;
		auto               decl_name = name(sym_id);

		result.push_back(
			searchForSymbolWithAttributeAndName<attributes::DVMOnlyImpl>(ctx, sym_id, decl_name)
		);
		result.push_back(
			searchForSymbolWithAttributeAndName<attributes::NativeOnlyImpl>(ctx, sym_id, decl_name)
		);
		return result;
	}

	void verifyBackendImplAttrUsage(query::Context& ctx, const HOUTFunctionDeclaration& fun_decl) {
		CORE_ASSERT(
			hasAttribute<attributes::DVMOnlyImpl>(fun_decl.original_symbol)
				|| hasAttribute<attributes::NativeOnlyImpl>(fun_decl.original_symbol),
			"Called with an invalid function declaration"
		);

		// First we check if we can find the declaration with the BackendDependent attribute.
		auto decl_sym = searchForSymbolWithAttributeAndName<attributes::BackendDependent>(
			ctx, fun_decl.original_symbol, fun_decl.original_name
		);
		auto& decl_fundecl = ctx.query<QueryDeclOfFun>(decl_sym)->valueOrThrow();

		auto comparison_result = compareFunctionDeclarations(
			ctx,
			decl_fundecl,
			fun_decl,
			{
				.compare_parameter_names = true,
				.compare_initial_values  = false,
				.log_error_on_mismatch   = true,
				.reason                  = base::strConcat(
                    "Attribute `",
                    attrNameStr(attributes::BackendDependent{}),
                    "` requires the function declaration to match the declaration with the `",
                    attrNameStr(attributes::DVMOnlyImpl{}),
                    "` or `",
                    attrNameStr(attributes::NativeOnlyImpl{}),
                    "` attribute."
                ),
			}
		);
		if (not comparison_result) {
			// Error logged by the `compareFunctionDeclarations` call.
			query::throwFailed();
		}
		std::string error_reason = "Attributes does not allow initial values for parameters.";
		if (ensureFunctionDeclarationNoInitialValue(ctx, fun_decl, error_reason).isBad()) {
			// Error logged by the `ensureFunctionDeclarationNoInitialValue` call.
			query::throwFailed();
		}
	}

	void verifyBackendDependentAttrUsage(
		query::Context& ctx, const HOUTFunctionDeclaration& fun_decl
	) {
		// We don't check here if the declaration matches with counterparts, as it is done in the
		// `DVMOnlyImpl` and `NativeOnlyImpl` declarations verification.

		std::string error_reason = "Attributes does not allow initial values for parameters.";
		if (ensureFunctionDeclarationNoInitialValue(ctx, fun_decl, error_reason).isBad()) {
			// Error logged by the `ensureFunctionDeclarationNoInitialValue` call.
			query::throwFailed();
		}
		// We discard the value here on purpose, only to throw errors.
		auto _ = getBackendDependentImplementations(ctx, fun_decl.original_symbol);
	}
}
