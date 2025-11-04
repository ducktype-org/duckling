#include "queries.hpp"

#include "abstract_type_impl.hpp"

#include <helios/symbols/query_class_symbol_data.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>

#include <query_framework/query_impl.hpp>

namespace tsh::internal {
	struct IMPLEMENT_QUERY(QueryInterfaceOfClass, TypeInterface) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			const compiler::helios::SymID symbol = key.value->getSymbol();
			auto& field_syms = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
			                       ->expect("Handling ERRORS in TS is not supported yet...")
			                       .members;
			// @TODO: #1485 Add methods to the interface, when obtaining their signature is supported.

			std::vector<InterfaceElement> elements;

			u32 declaration_order = 0;
			for (const compiler::helios::QueryTypeOfSymbol::QKey field_sym: field_syms) {
				const SymbolType<> field_type
					= ctx.query<compiler::helios::QueryTypeOfSymbol>(field_sym)->expect(
						"Handling ERRORS in TS is not supported yet..."
					);
				elements.push_back(InterfaceElement(
					field_sym, key.value->toAbstractType(), declaration_order, {}, field_type, {}
				));
				declaration_order++;
			}

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfClass)
}
