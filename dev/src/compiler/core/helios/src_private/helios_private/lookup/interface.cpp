#include "interface.hpp"

#include <helios/tsh/type_interface.hpp>
#include <helios_private/lookup/errors.hpp>
#include <helios_private/lookup/lookup_in_type_interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	CRef<query::QResult<LookupResult>> HInterface::lookup(
		query::Context& ctx, base::StrID name, AdditionalLookupParameters params
	) const {
		variant_match(data) {
			variant_case(ScopeInterface, scope) {
				return ctx.query<QueryLookupInScope>({ scope.scope, name, params.with_wildcards });
			}
			variant_case(ScopeWithParentsInterface, scope) {
				return ctx.query<QueryLookupInScopeAndParents>(
					{ scope.scope, name, params.with_wildcards }
				);
			}
			variant_case(SymbolInterface, symbol) {
				return ctx.query<QueryLookupInSymbol>({ symbol.symbol, name, params.with_wildcards }
				);
			}
			variant_case(TypeInstanceInterface, type) {
				return ctx.query<QueryLookupInType>(
					{ type.type, name, TypeAccessMode::Instance, params.accessing_scope }
				);
			}
			variant_case(TypeMetaInterface, type) {
				return ctx.query<QueryLookupInType>(
					{ type.type, name, TypeAccessMode::Meta, params.accessing_scope }
				);
			}
			variant_case(CustomInterface, custom) {
				return custom.custom->lookup(ctx, name, params);
			}
		}
		CORE_UNREACHABLE();
	}

	query::QResult<SymbolList> HInterface::lookupExpectUnique(
		const dia::StablePosition  error_position,
		query::Context&            ctx,
		base::StrID                name,
		AdditionalLookupParameters params
	) const {
		UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup(ctx, name, params));

		auto get_as_single = lookup_result->getAsSingle();

		if (get_as_single.hasFailed()) return query::Failed();

		variant_match(get_as_single.valueOrThrow()) {
			variant_case(SymbolList, symbol_list) {
				SymbolList dealiased_result;

				for (auto path_symbol: symbol_list) {
					UNPACK_QRESULT(const auto& dealiased =, *ctx.query<QueryDealias>(path_symbol));
					dealiased_result.appendList(dealiased);
				}

				return dealiased_result;
			}
			variant_case(errors::Ambiguity, _) {
				auto msg = makeBox<ShadowedVariableLookupError>(error_position);
				for (auto& leaf: lookup_result->leaves) {
					if_opt_some(getSymRef(leaf)->maybePstElement(), pst_elem) {
						auto decl_pos = pst_elem.unlock(ctx)->getStablePosition();
						msg->addAttachedMessage(makeBox<ShadowingDeclarationNote>(decl_pos));
					}
				}
				ctx.logInt(std::move(msg));
				return query::Failed();
			}
			variant_case(errors::Inaccessible, _) {
				auto msg = makeBox<InaccessibleSymbolLookupError>(error_position);
				for (auto& hidden: lookup_result->inaccessible) {
					if_opt_some(getSymRef(hidden)->maybePstElement(), pst_elem) {
						auto decl_pos = pst_elem.unlock(ctx)->getStablePosition();
						msg->addAttachedMessage(makeBox<InaccessibleDeclarationNote>(decl_pos));
					}
				}
				ctx.logInt(std::move(msg));
				return query::Failed();
			}
			variant_case(errors::SymbolNotFound, _) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat("Symbol '", name, "' not found in lookup"),
					error_position,
					"",
					"symbol lookup here"
				));
				return query::Failed();
			}
			variant_default { CORE_PANIC("Invalid state"); }
		}
		CORE_UNREACHABLE();
	}
}
