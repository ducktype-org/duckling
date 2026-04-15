#include "interface.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/lookup/errors.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

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
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryLookupInTypeInstance,
		KeyOf_LookupInTypeInstance,
		CRef<query::QResult<LookupResult>>,
		({})
	)

	struct IMPLEMENT_QUERY(QueryLookupInTypeInstance, query::QResult<LookupResult>) {
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
				return ctx.query<QueryLookupInTypeInstance>({ type.type, name });
			}
			variant_case(TypeMetaInterface, type) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Type meta lookups are not implemented yet", std::nullopt
				));
				static query::QResult<LookupResult> failed_result = query::Failed();
				return &failed_result;
			}
			variant_case(CustomInterface, custom) {
				return custom.custom->lookup(ctx, name, params);
			}
		}
		CORE_UNREACHABLE();
	}

	query::QResult<SymbolList> HInterface::lookupExpectUnique(
		dia::SourcePosition        error_position,
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
					if_opt_some(getSymRef(leaf)->getPSTDataOpt(), pst_data) {
						auto decl_pos = pst_data->getElement().unlock(ctx)->getSourcePosition();
						msg->addAttachedMessage(makeBox<ShadowingDeclarationNote>(decl_pos));
					}
				}
				ctx.logInt(std::move(msg));
				return query::Failed();
			}
			variant_case(errors::SymbolNotFound, _) {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
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
