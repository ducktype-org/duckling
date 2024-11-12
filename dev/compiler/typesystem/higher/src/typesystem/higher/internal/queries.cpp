#include "queries.hpp"

#include <query_framework/query_impl.hpp>

#include "type_info_impl.hpp"

namespace tsh::internal {
	struct IMPLEMENT_QUERY(QueryInterfaceOfClass, TypeInterface) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			compiler::helios::SymID symbol = key.value->getSymbol();
			auto& field_syms = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
			                       .expect("Handling ERRORS in TS is not supported yet...")
			                       .members;
			// @TODO: Add methods to the interface, when obtaining their signature is supported.

			std::set<InterfaceElement> elements;

			for (auto field_sym: field_syms) {
				TypeInfo field_type
					= ctx.query<compiler::helios::QueryTypeOfSymbol>(field_sym).expect(
						"Handling ERRORS in TS is not supported yet..."
					);
				elements.insert(
					InterfaceElement(field_sym, key.value->toTypeInfo(), {}, field_type, {})
				);
			}

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfClass)
}
