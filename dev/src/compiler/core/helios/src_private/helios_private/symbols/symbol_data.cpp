#include "symbol_data.hpp"

#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>

namespace compiler::helios::houtgen {
	tsh::SymbolType<> GeneratedSymbolData::getType(query::Context& ctx) const {
		variant_match(data) {
			variant_case(ImplicitConstructor, ctor) {
				const auto class_type = ctx.query<QueryTypeFromDefinition>({ ctor.class_symbol })
				                            ->expect(
												"Not handling errors here yet... "
												"(getting type of generated constructor symbol)"
											)
				                            .getType()
				                            .as<tsh::ClassAbstractType>();

				// @TODO: #1328 Properly handle value categories in class constructors.
				auto class_fields = class_type.getInterface(ctx).getFieldsView();
				std::vector<tsh::SymbolType<>> param_types;
				for (const auto& field: class_fields) param_types.push_back(field.getType(ctx));

				const tsh::SymbolType<> return_type{
					class_type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				const auto ctor_abstract_type = ctx.query<tsh::QueryFunctionType>({
					std::move(param_types),
					return_type,
				});

				return tsh::SymbolType<>{
					ctor_abstract_type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case(Parameter, param) {
				const auto function_type = ctx.query<QueryTypeOfSymbol>({ param.function_symbol })
				                               ->expect(
												   "Not handling errors here yet... "
												   "(getting type of generated parameter symbol)"
											   )
				                               .getType()
				                               .as<tsh::FunctionAbstractType>();
				return tsh::SymbolType{
					function_type.getParameterTypes().at(param.parameter_index).getType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case(Variable, var) { return var.type; }
		}
		CORE_UNREACHABLE();
	}
}
