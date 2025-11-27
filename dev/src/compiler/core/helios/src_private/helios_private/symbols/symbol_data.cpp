#include "symbol_data.hpp"

#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <typesystem/higher/queries/types.hpp>

namespace compiler::helios {
	namespace houtgen {
		base::Bit256 GeneratedSymbolData::ImplicitConstructor::queryUnstablePerfectHash() const {
			return { class_symbol.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::Parameter::queryUnstablePerfectHash() const {
			return { function_symbol.queryUnstablePerfectHash(), parameter_index };
		}

		base::Bit256 GeneratedSymbolData::Variable::queryUnstablePerfectHash() const {
			return { function_symbol.queryUnstablePerfectHash(), variable_index };
		}

		GeneratedSymbolData::GeneratedSymbolData(
			const std::variant<ImplicitConstructor, Parameter, Variable>& data
		):
			  data(data) {}

		base::Bit256 GeneratedSymbolData::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256, void>(
				data.index(), VISIT(data, d, return d.queryUnstablePerfectHash();)
			);
		}

		tsh::SymbolType<> GeneratedSymbolData::getType(query::Context& ctx) const {
			variant_match(data) {
				variant_case(ImplicitConstructor, ctor) {
					const auto class_type
						= ctx.query<QueryTypeFromDefinition>({ ctor.class_symbol })
					          ->expect(
								  "Not handling errors here yet... "
								  "(getting type of generated constructor symbol)"
							  )
					          .getType()
					          .as<tsh::ClassAbstractType>();

					// @TODO: #1328 Properly handle value categories in class constructors.
					auto class_fields = class_type.getInterface(ctx)->getFieldsView();
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
					const auto function_type
						= ctx.query<QueryTypeOfSymbol>({ param.function_symbol })
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

	SymbolData SymbolData::makePSTSymbolData(
		const CommonSymbolData common_data, PstSymbolData pst_data
	) {
		return SymbolData{
			.common = common_data,
			.other  = pst_data,
		};
	}

	SymbolData SymbolData::makeBuiltinFunction(
		const base::StrID name, builtin::BuiltinFunctionData builtin_data
	) {
		return SymbolData{
			.common = {
				.name = name,
				.kind = SymbolKind::BuiltinFunction,
			},
			.other  = builtin_data,
		};
	}

	SymbolData SymbolData::makeGeneratedSymbol(
		const base::StrID name, houtgen::GeneratedSymbolData generated_data
	) {
		SymbolKind kind{};
		variant_match(generated_data.data) {
			variant_case_novalue(houtgen::GeneratedSymbolData::ImplicitConstructor) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(houtgen::GeneratedSymbolData::Parameter) {
				kind = SymbolKind::Parameter;
			}
			variant_case_novalue(houtgen::GeneratedSymbolData::Variable) {
				kind = SymbolKind::Variable;
			}
			variant_default { CORE_UNREACHABLE(); }
		}
		return SymbolData{
			.common = {
				.name = name,
				.kind = kind,
			},
			.other  = generated_data,
		};
	}
}
