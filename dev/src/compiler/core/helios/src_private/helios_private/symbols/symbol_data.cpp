#include "symbol_data.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>

#include <base/except/exceptions.hpp>

namespace compiler::helios {
	namespace defgen {
		base::Bit256 GeneratedSymbolData::ImplicitConstructor::queryUnstablePerfectHash() const {
			return { class_symbol.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultClassConstructor::queryUnstablePerfectHash() const {
			return { class_symbol.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultStaticArrayConstructor::queryUnstablePerfectHash(
		) const {
			return { array_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::ToStringMethod::queryUnstablePerfectHash() const {
			return owner_type.queryUnstablePerfectHash();
		}

		base::Bit256 GeneratedSymbolData::DefaultDestructor::queryUnstablePerfectHash() const {
			return owner_type.queryUnstablePerfectHash();
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

		base::Bit256 GeneratedSymbolData::SelfParameterNoScope::queryUnstablePerfectHash() const {
			return { method_symbol.queryUnstablePerfectHash() };
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

		base::Bit256 GeneratedSymbolData::ScriptMainWrapper::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(script_id, scope.queryUnstablePerfectHash());
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
					const auto class_type
						= ctx.query<QueryTypeFromDefinition>({ ctor.class_symbol })
					          ->valueOrThrow()
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

					const auto ctor_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ {}, return_type });

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

					const auto ctor_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ {}, return_type });

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(ToStringMethod, to_string) {
					const tsh::SymbolType<> self_type{
						to_string.owner_type,
						tsh::ReferenceKind::Ref,
						tsh::Mutability::Immutable,
					};

					const tsh::SymbolType<> return_type{
						tsh::getStringType(),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					};

					const auto to_string_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ { self_type }, return_type });

					return tsh::SymbolType<>{
						to_string_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(DefaultDestructor, dtor) {
					const tsh::SymbolType<> self_type{
						dtor.owner_type,
						tsh::ReferenceKind::Ref,
						tsh::Mutability::Mutable,
					};

					const tsh::SymbolType<> return_type{
						tsh::getUnitType(),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};

					const auto dtor_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ { self_type }, return_type });

					return tsh::SymbolType<>{
						dtor_abstract_type,
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
				variant_case(SelfParameterNoScope, param) {
					// Don't use QueryClassOfMember (if it even still exists) because methods may be
					// generated for non-class types, and QueryClassOfMember is a temporary solution anyway.
					// Reconsidevisit
					const auto method_type = ctx.query<QueryTypeOfSymbol>({ param.method_symbol })
					                             ->valueOrThrow()
					                             .getType()
					                             .as<tsh::FunctionAbstractType>();
					const auto& self_type = method_type.getParameterTypes().front();
					return self_type;
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
				variant_case_novalue(ReplInstructionWrapper) {
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
				variant_case_novalue(ScriptMainWrapper) {
					const auto return_type = tsh::SymbolType<>{
						tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					};
					const auto function_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ {}, return_type });
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
				variant_case(SelfParameterNoScope, param) {
					CORE_PANIC("Can't get scope of generated self parameter without scope.");
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
				variant_case(ScriptMainWrapper, script) { return script.scope; }
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
				variant_case(SelfParameterNoScope, param) { return {}; }
				variant_case(Variable, var) { return {}; }
				variant_case(ReplExpressionWrapper, repl) { return {}; }
				variant_case(ReplInstructionWrapper, repl) { return {}; }
				variant_case(ScriptMainWrapper, script) { return script.scope; }
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
			variant_case_novalue(defgen::GeneratedSymbolData::ToStringMethod) {
				kind = SymbolKind::Method;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::DefaultDestructor) {
				kind = SymbolKind::Method;
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
			variant_case_novalue(defgen::GeneratedSymbolData::SelfParameterNoScope) {
				kind = SymbolKind::Parameter;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::Variable) {
				kind = SymbolKind::Variable;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::ReplExpressionWrapper) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::ReplInstructionWrapper) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::ScriptMainWrapper) {
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
