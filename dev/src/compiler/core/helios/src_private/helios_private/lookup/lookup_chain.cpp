
#include "lookup_chain.hpp"

#include "helios_private/symbols/symbols.hpp"

#include <helios_private/lookup/interface.hpp>

#include "base/extend_cpp/variant_match.hpp"

namespace compiler::helios {


	query::QResult<SymbolList> lookupChain(query::Context& ctx, const LookupChainKey& key) {
		CORE_ASSERT(!key.names.empty(), "lookupChain received zero names");

		bool       first_symbol = true;
		SymbolList result;
		for (auto pointed: key.names) {
			auto lookup_interface = first_symbol ? HInterface::ofScopeWithParents(key.begin_scope)
			                                     : HInterface::ofSymbol(result.back());

			UNPACK_QRESULT_CREF(
				CRef<LookupResult> lookup_result = &,
				lookup_interface.lookup(ctx, pointed.value, key.params)
			);


			UNPACK_QRESULT(auto get_as_single =, lookup_result->getAsSingle());

			variant_match(get_as_single) {
				variant_case(SymbolList, symbol_list) {
					SymbolList dealiased_result;

					for (auto path_symbol: symbol_list) {
						UNPACK_QRESULT(
							const auto& dealiased =, *ctx.query<QueryDealias>(path_symbol)
						);
						dealiased_result.appendList(dealiased);
					}

					result.appendList(dealiased_result);
				}
				variant_case(errors::Ambiguity, _) {
					ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						base::strConcat("Symbol '", pointed.value, "' is ambiguous in lookup chain"),
						pointed.position
					));
					return query::Failed();
				}
				variant_case(errors::SymbolNotFound, _) {
					ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						base::strConcat("Symbol '", pointed.value, "' not found in lookup chain"),
						pointed.position
					));
					return query::Failed();
				}
				variant_default { CORE_PANIC("Invalid state"); }
			}

			first_symbol = false;
		}
		return result;
	}
}
