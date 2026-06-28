#include "queries.hpp"

#include "abstract_type_impl.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::tsh {
	struct IMPLEMENT_QUERY(QueryInterfaceOfClass, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			const compiler::helios::SymID symbol = key.value->getSymbol();

			const auto& class_data
				= ctx.query<compiler::helios::QueryClassSymbolData>(symbol)->valueOrThrow();

			std::vector<InterfaceElement> elements;
			elements.reserve(class_data.members.size() + class_data.methods.size());

			u32 declaration_order = 0;
			for (const compiler::helios::SymID field_sym: class_data.members) {
				elements.push_back(InterfaceElement(
					field_sym,
					key.value->toAbstractType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					{}
				));
				declaration_order++;
			}

			for (const compiler::helios::SymID method_sym: class_data.methods) {
				elements.push_back(InterfaceElement(
					method_sym,
					key.value->toAbstractType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Method,
					{}
				));
				declaration_order++;
			}

			for (const compiler::helios::SymID ctor_sym: class_data.constructors) {
				if (!compiler::helios::isUserDefinedCopyConstructor(ctx, ctor_sym)) continue;
				elements.push_back(InterfaceElement(
					ctor_sym,
					key.value->toAbstractType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Method,
					{}
				));
				declaration_order++;
			}

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfClass)

	struct IMPLEMENT_QUERY(QueryInterfaceOfTuple, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			std::vector<InterfaceElement> elements;
			elements.reserve(key.value->getComponents().size());

			auto components = ctx.query<helios::QueryTupleTypeData>(key.value->toAbstractType())
			                      ->valueOrThrow()
			                      .members;
			u32 declaration_order = 0;
			for (const auto& component: components) {
				elements.emplace_back(
					component,
					ctx.query<helios::QueryTypeOfSymbol>(component)->valueOrThrow().getType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					ClassMemberVisibility::Public
				);
				declaration_order++;
			}


			auto interface = TypeInterface(elements);
			return interface;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfTuple)
}
