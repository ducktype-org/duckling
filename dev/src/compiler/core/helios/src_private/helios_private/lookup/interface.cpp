#include "interface.hpp"

#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::helios {

	struct KeyOf_LookupInTypeInstance final {
		tsh::AbstractType type;
		base::StrID       name;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return { type.queryUnstablePerfectHash(), static_cast<u64>(name), 0, 0 };
		}
	};


	/**
	 * This query is placed here, to keep it close to HInterface::lookup.
	 * In #1477 and/or #1392 it should be placed in a more appropriate location.
	 *
	 * See https://docs.duckling.pl/duckling/lookup/name_lookup.html
	 * for more info on type-instance lookups.
	 */
	DECLARE_QUERY(QueryLookupInTypeInstance, KeyOf_LookupInTypeInstance, CRef<LookupResult>)

	struct IMPLEMENT_QUERY(QueryLookupInTypeInstance, LookupResult) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			// @TODO: #1412 #1531 this a mock that works for now, make it better

			auto        interface = key.type.getInterface(ctx);
			const auto& elements  = interface->getElementsWithName(key.name);

			LookupResult result;
			for (const auto& element: elements) result.leaves.emplace_back(element.getSymbol());

			return result;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInTypeInstance);

	CRef<LookupResult> HInterface::lookup(
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
				return ctx.query<QueryLookupInTypeInstance>({ type.type, name });
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
	) const {
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
