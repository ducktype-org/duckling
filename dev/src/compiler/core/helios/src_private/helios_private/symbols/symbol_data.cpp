#include "symbol_data.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>

#include "base/str/str_utils.hpp"
#include <base/except/exceptions.hpp>
#include "helios/tsh/types.hpp"

namespace compiler::helios {
	namespace defgen {
		base::Bit256 GeneratedSymbolData::ImplicitConstructor::queryUnstablePerfectHash() const {
			return { class_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultClassConstructor::queryUnstablePerfectHash() const {
			return { class_symbol.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultStaticArrayConstructor::queryUnstablePerfectHash(
		) const {
			return { array_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::BuiltinOperator::queryUnstablePerfectHash() const {
			return { operator_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::Parameter::queryUnstablePerfectHash() const {
			return { function_symbol.queryUnstablePerfectHash(), parameter_index };
		}

		base::Bit256 GeneratedSymbolData::SelfParameter::queryUnstablePerfectHash() const {
			return { method_symbol.queryUnstablePerfectHash(), scope.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::Field::queryUnstablePerfectHash() const {
			return { parent_type.queryUnstablePerfectHash(), index };
		}

		base::Bit256 GeneratedSymbolData::Variable::queryUnstablePerfectHash() const {
			return { function_symbol.queryUnstablePerfectHash(), variable_index };
		}

		// @TODO: #1807 Refactor the code so that it's impossible to create
		// two symbols with the same counter but different return types.
		base::Bit256 GeneratedSymbolData::ReplExpressionWrapper::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(return_type, counter);
		}

		base::Bit256 GeneratedSymbolData::ReplInstructionWrapper::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(counter);
		}

		GeneratedSymbolData::GeneratedSymbolData(const GeneratedSymbolDataVariant& data):
			  data(data) {}

		base::Bit256 GeneratedSymbolData::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				data.index(), VISIT(data, d, return d.queryUnstablePerfectHash();)
			);
		}

		tsh::SymbolType<> GeneratedSymbolData::getType(query::Context& ctx) const {
			variant_match(data) {
				variant_case(ImplicitConstructor, ctor) {
					const auto class_type = ctor.class_type;

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
				variant_case(DefaultClassConstructor, ctor) {
					const auto class_type
						= ctx.query<QueryTypeFromDefinition>({ ctor.class_symbol })
					          ->valueOrThrow()
					          .getType()
					          .as<tsh::ClassAbstractType>();

					// @TODO: #1328 Properly handle value categories in class constructors.
					const tsh::SymbolType<> return_type{
						class_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					};

					const auto ctor_abstract_type = ctx.query<tsh::QueryFunctionType>({
						{},
						return_type,
					});

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(DefaultStaticArrayConstructor, ctor) {
					const tsh::SymbolType<> return_type{
						ctor.array_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					};

					const auto ctor_abstract_type = ctx.query<tsh::QueryFunctionType>({
						{},
						return_type,
					});

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(BuiltinOperator, op) {
					return tsh::SymbolType<>{
						op.operator_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(Parameter, param) {
					const auto function_type
						= ctx.query<QueryTypeOfSymbol>({ param.function_symbol })
					          ->valueOrThrow()
					          .getType()
					          .as<tsh::FunctionAbstractType>();
					auto param_symbol_type
						= function_type.getParameterTypes().at(param.parameter_index);
					return param_symbol_type;
				}
				variant_case(SelfParameter, param) {
					const auto class_type
						= ctx.query<QueryClassOfMember>(param.method_symbol)->valueOrThrow();
					auto param_symbol_type = tsh::SymbolType{
						class_type,
						tsh::ReferenceKind::Ref,
						tsh::Mutability::Mutable,
					};
					return param_symbol_type;
				}
				variant_case(Field, field) { 
					// @TODO: #2515 Implament other cases
					switch (field.parent_type.getKind()) {
					case tsh::Kind::Tuple:
						return field.parent_type.as<tsh::TupleAbstractType>().getComponents().at(field.index);
					default:
						throw base::NotYetImplemented(base::strConcat(
							"Can not get the type of a member of the ",
							base::enumToStr(field.parent_type.getKind()),
							" kind."
						));
					}
				}
				variant_case(Variable, var) { return var.type; }
				variant_case(ReplExpressionWrapper, repl) {
					const auto function_abstract_type = ctx.query<tsh::QueryFunctionType>({
						{},
						repl.return_type,
					});
					return tsh::SymbolType<>{
						function_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(ReplInstructionWrapper, repl) {
					// Unit (not Void) is the correct return type for procedures.
					// Per the language spec: "void ... cannot be returned from a function".
					const auto void_type = tsh::SymbolType<>{
						tsh::getUnitType(),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					};
					const auto function_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ {}, void_type });
					return tsh::SymbolType<>{
						function_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
			}
			CORE_UNREACHABLE();
		}

		ScopeID GeneratedSymbolData::getScope() const {
			variant_match(data) {
				variant_case(ImplicitConstructor, ctor) {
					CORE_PANIC("Can't get scope of implicit constructor yet.");
				}
				variant_case(DefaultClassConstructor, ctor) {
					CORE_PANIC("Can't get scope of implicit constructor yet.");
				}
				variant_case(DefaultStaticArrayConstructor, ctor) {
					CORE_PANIC("Can't get scope of implicit constructor yet.");
				}
				variant_case(BuiltinOperator, op) {
					CORE_PANIC("Can't get scope of builtin operator yet.");
				}
				variant_case(Parameter, param) {
					CORE_PANIC("Can't get scope of generated parameter yet.");
				}
				variant_case(SelfParameter, param) { return param.scope; }
				variant_case(Field, field) {
					CORE_PANIC("Can't get scope of generated field yet.");
				}
				variant_case(Variable, var) {
					CORE_PANIC("Can't get scope of generated variable yet.");
				}
				variant_case(ReplExpressionWrapper, repl) {
					CORE_PANIC("Can't get scope of repl expr wrapper yet.");
				}
				variant_case(ReplInstructionWrapper, repl) {
					CORE_PANIC("Can't get scope of repl instruction wrapper yet.");
				}
			}
			CORE_UNREACHABLE();
		}

		[[nodiscard]]
		base::Optional<ScopeID> GeneratedSymbolData::maybeScope() const {
			variant_match(data) {
				variant_case(ImplicitConstructor, ctor) { return {}; }
				variant_case(DefaultClassConstructor, ctor) { return {}; }
				variant_case(DefaultStaticArrayConstructor, ctor) { return {}; }
				variant_case(BuiltinOperator, op) { return {}; }
				variant_case(Parameter, param) { return {}; }
				variant_case(SelfParameter, param) { return param.scope; }
				variant_case(Field, field) { return {}; }
				variant_case(Variable, var) { return {}; }
				variant_case(ReplExpressionWrapper, repl) { return {}; }
				variant_case(ReplInstructionWrapper, repl) { return {}; }
				variant_default { CORE_PANIC("Unhandled symbol kind"); }
			}
			CORE_UNREACHABLE();
		}
	}

	SymbolData::SymbolData(CommonSymbolData common, OtherData other):
		  common(common),
		  other(other),
		  id(SymbolDataID::next()) {}

	SymbolData SymbolData::makePSTSymbolData(
		const CommonSymbolData common_data, PstSymbolData pst_data
	) {
		return { common_data, pst_data };
	}

	SymbolData SymbolData::makeBuiltinFunction(
		const base::StrID name, builtin::BuiltinFunctionData builtin_data
	) {
		return {
			{
				.name = name,
				.kind = SymbolKind::Function,
			},
			builtin_data,
		};
	}

	SymbolData SymbolData::makeGeneratedSymbol(
		const base::StrID name, defgen::GeneratedSymbolData generated_data
	) {
		SymbolKind kind{};
		variant_match(generated_data.data) {
			variant_case_novalue(defgen::GeneratedSymbolData::ImplicitConstructor) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::DefaultClassConstructor) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::DefaultStaticArrayConstructor) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::BuiltinOperator) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::Parameter) {
				kind = SymbolKind::Parameter;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::SelfParameter) {
				kind = SymbolKind::Parameter;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::Field) { kind = SymbolKind::Field; }
			variant_case_novalue(defgen::GeneratedSymbolData::Variable) {
				kind = SymbolKind::Variable;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::ReplExpressionWrapper) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::ReplInstructionWrapper) {
				kind = SymbolKind::Function;
			}
			variant_default { CORE_UNREACHABLE(); }
		}
		return {
			{
				.name = name,
				.kind = kind,
			},
			generated_data,
		};
	}
}
