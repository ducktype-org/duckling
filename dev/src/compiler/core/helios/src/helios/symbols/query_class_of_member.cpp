#include "query_class_of_member.hpp"

#include "helios/symbols/symbol_kind.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/scopes/scope_data.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "base/except/exceptions.hpp"

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryClassOfMember, SymID) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Constructor or kind(key) == SymbolKind::Destructor
					or kind(key) == SymbolKind::Field or kind(key) == SymbolKind::Method,
				"Expected a member of a class symbol."
			);
			auto scope_id = scope(key);
			auto pst_element
				= pst::LangElement::getByStableHash(scope_id.ref->related_pst_element_hash.value())
			          .unlock(ctx)
			          ->getParent()
			          .value()
			          .unlock(ctx);
			auto class_symbol = ctx.query<QuerySymbolOfSTMT>(pst_element).valueOrThrow();

			return class_symbol;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassOfMember);
}
