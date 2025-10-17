#include "interface.hpp"

#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/context.hpp>

namespace compiler::helios {

	CRef<LookupResult> HInterface::lookup(
		query::Context& ctx, base::StrID name, AdditionalLookupParameters params
	) {
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
				throw base::NotYetImplemented("HInterface::lookup for type instance");
			}
			variant_case(TypeMetaInterface, type) {
				throw base::NotYetImplemented("HInterface::lookup for type");
			}
			variant_case(CustomInterface, custom) {
				return custom.custom->lookup(ctx, name, params);
			}
		}
		CORE_UNREACHABLE();
	}

	query::QResult<SymbolList, errors::Failed> HInterface::lookupExpectUnique(
		dia::SourcePosition        error_position,
		query::Context&            ctx,
		base::StrID                name,
		AdditionalLookupParameters params
	) {
		auto lookup_result = lookup(ctx, name, params);
		auto get_as_single = lookup_result->getAsSingle();

		if (get_as_single.hasError()) {
			variant_match(get_as_single.error()) {
				variant_case(errors::Ambiguity, _) {
					ctx.log(dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::make(
						error_position, "Ambiguity in lookup"
					));
				}
				variant_case(errors::SymbolNotFound, _) {
					ctx.log(dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::make(
						error_position, "Symbol not found in lookup"
					));
				}
				variant_default { CORE_PANIC("Invalid state"); }
			}
			return query::QError(errors::Failed());
		}

		const auto& symbols = get_as_single.value();

		SymbolList dealiased_result;

		for (auto path_symbol: symbols) {
			UNPACK_RESULT(const auto& dealiased =, *ctx.query<QueryDealias>(path_symbol));
			dealiased_result.appendList(dealiased);
		}

		return dealiased_result;
	}
}
