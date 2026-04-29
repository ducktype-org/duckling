#include "query_class_of_member.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/scopes/scope_data.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	// @TODO: #2111 This is a temporary solution. Refactor class members symbol data to be able to
	// access the class directly from the symbol data, without having to go through the scope and
	// PST element.
	struct IMPLEMENT_QUERY(QueryClassOfMember, query::QResult<tsh::ClassAbstractType>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Constructor or kind(key) == SymbolKind::Destructor
					or kind(key) == SymbolKind::Field or kind(key) == SymbolKind::Method,
				"Expected a member of a class symbol."
			);
			auto scope_id = scope(key);
			auto pst_element
				= scope_id.ref->relatedPSTElement()->unlock(ctx)->getParent().value().unlock(ctx);
			auto class_symbol = ctx.query<QuerySymbolOfSTMT>(pst_element).valueOrThrow();
			auto class_type   = ctx.query<QueryTypeFromDefinition>(class_symbol)
			                      ->valueOrThrow()
			                      .getType()
			                      .as<tsh::ClassAbstractType>();

			return class_type;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassOfMember);
}
