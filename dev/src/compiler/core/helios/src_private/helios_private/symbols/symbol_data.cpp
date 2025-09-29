#include "symbol_data.hpp"

#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>

namespace compiler::helios::houtgen {
	tsh::SymbolType<> GeneratedSymbolData::getType(query::Context& ctx) const {
		variant_match(data) {
			variant_case(ImplicitConstructor, ctor) {
				const auto class_type = ctx.query<QueryTypeFromDefinition>({ ctor.classSymbol })
				                            ->expect(
												"Not handling errors here yet... (getting type of "
												"generated constructor symbol)"
											)
				                            .getType()
				                            .as<tsh::ClassAbstractType>();

				// @TODO: #1328 Properly handle value categories in class constructors.
				const tsh::SymbolType<> self_type{
					class_type,
					tsh::ReferenceKind::Ref,
					tsh::Mutability::Mutable,
				};
				auto        class_fields = class_type.getInterface(ctx).getFieldsView();
				std::vector param_types  = { self_type };
				for (const auto& field: class_fields) param_types.push_back(field.getType(ctx));

				const auto ctor_abstract_type = ctx.query<tsh::QueryFunctionType>({
					std::move(param_types),
					tsh::SymbolType<>{
						ctx.query<tsh::QueryUnitType>({}),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					},
				});

				return tsh::SymbolType<>{
					ctor_abstract_type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case(Variable, var) {
				const auto function_type
					= ctx.query<QueryTypeOfSymbol>({ var.functionSymbol })
				          ->expect(
							  "Not handling errors here yet... (getting type of generated "
							  "variable symbol)"
						  )
				          .getType()
				          .as<tsh::FunctionAbstractType>();
				return tsh::SymbolType{
					function_type.getParameterTypes().at(var.argumentIndex).getType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
		}
		CORE_UNREACHABLE();
	}
}
