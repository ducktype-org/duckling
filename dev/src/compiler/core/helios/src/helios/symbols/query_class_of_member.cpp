#include "query_class_of_member.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/scopes/scope_data.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryClassOfMember, SymID) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto scope_id = scope(key);
			auto pst_element
				= scope_id.ref->related_pst_element.value().unlock(ctx)->getParent().value().unlock(
					ctx
				);
			auto class_symbol = ctx.query<QuerySymbolOfSTMT>(pst_element);

			return class_symbol;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassOfMember);
}
