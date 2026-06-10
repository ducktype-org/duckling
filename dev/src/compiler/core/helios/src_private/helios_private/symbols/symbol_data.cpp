#include "symbol_data.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

namespace compiler::helios {
	namespace defgen {
		base::Bit256 GeneratedSymbolData::ImplicitConstructor::queryUnstablePerfectHash() const {
			return { target_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultClassConstructor::queryUnstablePerfectHash() const {
			return { class_symbol.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultTupleConstructor::queryUnstablePerfectHash(
		) const {
			return { tuple_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::DefaultStaticArrayConstructor::queryUnstablePerfectHash(
		) const {
			return { array_type.queryUnstablePerfectHash() };
		}

		base::Bit256 GeneratedSymbolData::ToStringMethod::queryUnstablePerfectHash() const {
			return owner_type.queryUnstablePerfectHash();
		}

		base::Bit256 GeneratedSymbolData::LengthMethod::queryUnstablePerfectHash() const {
			return { owner_type.queryUnstablePerfectHash() };
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

		base::Bit256 GeneratedSymbolData::Field::queryUnstablePerfectHash() const {
			return { parent_type.queryUnstablePerfectHash(), index };
		}

		base::Bit256 GeneratedSymbolData::GeneratedFunctionVariable::queryUnstablePerfectHash(
		) const {
			return { function_symbol.queryUnstablePerfectHash(), variable_index };
		}

		base::Bit256 GeneratedSymbolData::ControlFlowLocal::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(owning_scope.queryUnstablePerfectHash(), role);
		}

		// Hash includes both counter and return_type to ensure different wrappers are distinguished.
		// However, the mangled name (used for linker symbols) is based only on counter.
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
					const auto target_type = ctor.target_type;

					// @TODO: #1328 Properly handle value categories in class constructors.
					auto fields = target_type.getInterface(ctx)->getFieldsView();
					std::vector<tsh::SymbolType<>> param_types;
					for (const auto& field: fields) param_types.push_back(field.getType(ctx));

					const auto return_type = tsh::SymbolType<>::withDefaults(target_type);

					const auto ctor_abstract_type = ctx.query<tsh::QueryFunctionType>({
						.parameter_types = std::move(param_types),
						.result_type     = return_type,
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
					const auto return_type = tsh::SymbolType<>::withDefaults(class_type);

					const auto ctor_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = {},
					                                          .result_type     = return_type });

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(DefaultTupleConstructor, ctor) {
					const auto return_type = tsh::SymbolType<>::withDefaults(ctor.tuple_type);

					const auto ctor_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = {},
					                                          .result_type     = return_type });

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(DefaultStaticArrayConstructor, ctor) {
					const auto return_type = tsh::SymbolType<>::withDefaults(ctor.array_type);

					const auto ctor_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = {},
					                                          .result_type     = return_type });

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(ToStringMethod, to_string) {
					const tsh::SymbolType<> self_type{
						to_string.owner_type,
						to_string.owner_type.isSimple() ? tsh::ReferenceKind::Direct
														: tsh::ReferenceKind::Ref,
						tsh::Mutability::Immutable,
					};

					const auto return_type = tsh::SymbolType<>::withDefaults(tsh::getStringType());

					const auto to_string_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = { self_type },
					                                          .result_type     = return_type });

					return tsh::SymbolType<>{
						to_string_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(LengthMethod, length_method) {
					const tsh::SymbolType<> self_type{
						length_method.owner_type,
						length_method.owner_type.isSimple() ? tsh::ReferenceKind::Direct
															: tsh::ReferenceKind::Ref,
						tsh::Mutability::Immutable,
					};

					const auto return_type = tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
						ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned
					));

					const auto length_method_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = { self_type },
					                                          .result_type     = return_type });

					return tsh::SymbolType<>{
						length_method_abstract_type,
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
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = { self_type },
					                                          .result_type     = return_type });

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
				variant_case(Field, field) {
					// @TODO: #2515 Implement other cases
					switch (field.parent_type.getKind()) {
					case tsh::Kind::Tuple:
						return field.parent_type.as<tsh::TupleAbstractType>().getComponents().at(
							field.index
						);
					case tsh::Kind::Slice: {
						if (field.index == 0) {
							auto element_type
								= field.parent_type.as<tsh::SliceAbstractType>().getElementType();
							auto many_pointer_type
								= ctx.query<tsh::QueryManyPointerType>({ element_type });
							return tsh::SymbolType<>::withDefaults(many_pointer_type);
						}
						if (field.index == 1) {
							return tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
								ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned
							));
						}
						CORE_PANIC("Slice only has fields 0 (element) and 1 (length)");
					}
					default:
						CORE_UNREACHABLE();
					}
				}
				variant_case(GeneratedFunctionVariable, var) { return var.type; }
				variant_case(ControlFlowLocal, local) { return local.type; }
				variant_case(ReplExpressionWrapper, repl) {
					const auto function_abstract_type = ctx.query<tsh::QueryFunctionType>({
						.parameter_types = {},
						.result_type     = repl.return_type,
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
					const auto void_type = tsh::SymbolType<>::withDefaults(tsh::getUnitType());
					const auto function_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = {},
					                                          .result_type     = void_type });
					return tsh::SymbolType<>{
						function_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case_novalue(ScriptMainWrapper) {
					const auto return_type = tsh::SymbolType<>::withDefaults(
						tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
					);
					const auto function_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = {},
					                                          .result_type     = return_type });
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
				variant_case(DefaultTupleConstructor, ctor) {
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
				variant_case(GeneratedFunctionVariable, var) {
					CORE_PANIC("Can't get scope of generated variable yet.");
				}
				variant_case(ControlFlowLocal, local) { return local.owning_scope; }
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
				variant_case(DefaultTupleConstructor, ctor) { return {}; }
				variant_case(DefaultStaticArrayConstructor, ctor) { return {}; }
				variant_case(DefaultDestructor, dtor) { return {}; }
				variant_case(ToStringMethod, to_string) { return {}; }
				variant_case(LengthMethod, m) { return {}; }
				variant_case(BuiltinOperator, op) { return {}; }
				variant_case(Parameter, param) { return {}; }
				variant_case(SelfParameter, param) { return param.scope; }
				variant_case(Field, field) { return {}; }
				variant_case(GeneratedFunctionVariable, var) { return {}; }
				variant_case(ControlFlowLocal, var) { return getScope(); }
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

	SymbolData SymbolData::makeGeneratedSymbol(
		const base::StrID name, defgen::GeneratedSymbolData generated_data
	) {
		SymbolKind kind{};
		variant_match(generated_data.data) {
			variant_case_novalue(
				defgen::GeneratedSymbolData::ImplicitConstructor,
				defgen::GeneratedSymbolData::DefaultClassConstructor,
				defgen::GeneratedSymbolData::DefaultTupleConstructor,
				defgen::GeneratedSymbolData::DefaultStaticArrayConstructor,
				defgen::GeneratedSymbolData::BuiltinOperator,
				defgen::GeneratedSymbolData::ReplExpressionWrapper,
				defgen::GeneratedSymbolData::ReplInstructionWrapper,
				defgen::GeneratedSymbolData::ScriptMainWrapper,
				defgen::GeneratedSymbolData::ToStringMethod,
				defgen::GeneratedSymbolData::LengthMethod,
				defgen::GeneratedSymbolData::DefaultDestructor
			) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(
				defgen::GeneratedSymbolData::Parameter, defgen::GeneratedSymbolData::SelfParameter
			) {
				kind = SymbolKind::Parameter;
			}
			variant_case_novalue(defgen::GeneratedSymbolData::Field) { kind = SymbolKind::Field; }
			variant_case_novalue(
				defgen::GeneratedSymbolData::GeneratedFunctionVariable,
				defgen::GeneratedSymbolData::ControlFlowLocal
			) {
				kind = SymbolKind::Variable;
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
