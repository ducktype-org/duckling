#include "builders.hpp"

#include "base/maps.hpp"
#include "base/string_id.hpp"
#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include "vm/core/process/type_metadata/definitions.hpp"
#include "vm/core/process/type_metadata/type.hpp"
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/builders/function_validator.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <algorithm>
#include <functional>
#include <ranges>
#include <variant>

using namespace vm::code::builders;

FunctionBuilder::FunctionBuilder(
	vm::code::Identifier name, const GlobalDataMap& globals, const TypeContext& types
):
	  name(name),
	  type_context(types),
	  globals(globals) {}

vm::code::Function FunctionBuilder::build() const {
	FunctionValidator validator(name, type_context, globals, instructions);
	validator.validate();
	Function function;
	function.body = validator.extractReachableCode();
	function.name = name;
	return function;
}

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	instructions.push_back(instruction);
}

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& vm::code::builders::TypeContextBuilder::getTypes(
) const {
	return types;
}

void TypeContextBuilder::validateType(const TypeOfData& type) const {
	auto get_type = [&](base::StrID name) {
		return types.atMaybe(name).expect<UnknownSubtypeError>(type, name);
	};
	// auto get_type = [&]<typename T>(base::StrID name, auto error_factory) -> T {
	// 	auto type_variant = get_type_variant(name);
	// 	if (!std::holds_alternative<T>(type_variant)) error_factory();
	// 	return std::get<T>(type_variant);
	// };

	// Validates if implements contain interfaces.
	auto validate_implements = [&](const std::vector<base::StrID>& implements) {
		base::HashMap<base::StrID, base::StrID> interfaces;
		for (const auto& impl: implements) {
			if (interfaces.contains(impl)) throw DuplicatedImplementsError(type);
			interfaces.put(impl);
			auto impl_type = get_type(impl);
			if (!std::holds_alternative<InterfaceType>(*impl_type) || *impl_type == type)
				throw InvalidImplementsError(type);
		}
	};

	// TODO: Merge those two into one.
	auto validate_first_argument = [&](const ClassType& clazzz, const FunctionType& func) {
		if (func.parameters.size() == 0) throw MethodFirstArgumentError(clazzz, func.name);

		auto first_param_type = *get_type(func.parameters[0]);
		if (!std::holds_alternative<PointerType>(first_param_type))
			throw MethodFirstArgumentError(clazzz, func.name);
		auto first_param = std::get<PointerType>(first_param_type);

		if (first_param.inner != clazzz.name) throw MethodFirstArgumentError(clazzz, func.name);
	};

	auto validate_first_argument_iface = [&](const InterfaceType& iface, const FunctionType& func) {
		if (func.parameters.size() == 0) throw MethodFirstArgumentError(iface, func.name);

		auto first_param_type = *get_type(func.parameters[0]);
		if (!std::holds_alternative<PointerType>(first_param_type))
			throw MethodFirstArgumentError(iface, func.name);
		auto first_param = std::get<PointerType>(first_param_type);

		if (first_param.inner != iface.name) throw MethodFirstArgumentError(iface, func.name);
	};

	auto validate_signature_match
		= [&](const ClassType& clazzz, const FunctionType& vmethod, const FunctionType& impl) {
			  if (vmethod.result != impl.result) throw MethodTypeError(clazzz, impl.name);
			  if (vmethod.parameters.size() != impl.parameters.size())
				  throw MethodTypeError(clazzz, impl.name);

			  for (u64 i = 1; i < impl.parameters.size(); i++)
				  if (impl.parameters[i] != vmethod.parameters[i])
					  throw MethodTypeError(clazzz, impl.name);
		  };

	auto validate_signature_match_iface
		= [&](const InterfaceType& iface, const FunctionType& vmethod, const FunctionType& impl) {
			  if (vmethod.result != impl.result) throw MethodTypeError(iface, impl.name);
			  if (vmethod.parameters.size() != impl.parameters.size())
				  throw MethodTypeError(iface, impl.name);

			  for (u64 i = 1; i < impl.parameters.size(); i++)
				  if (impl.parameters[i] != vmethod.parameters[i])
					  throw MethodTypeError(iface, impl.name);
		  };
	// TODO: What is someone declares a field named vt.
	auto validate_implementations = [&](const ClassType& clazz) {
		// Validates if declared implementations have corresponding virtual method declarations.
		// Validates signatures of implementations.
		base::HashMap<base::StrID, base::StrID> virtual_methods;

		// Collect all virtual methods that can be implemented by this class.
		std::function<void(const InterfaceType&)> collect_virtual_methods_iface
			= [&](const InterfaceType& ifacee) {
				  for (const auto& iface: ifacee.implements) {
					  //@note: We are guaranteed an InterfaceType is contained. This was checked before.
					  const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
					  // TODO: Change that to invoke collect_virtual_methods.
					  collect_virtual_methods_iface(iface_type);
				  }
				  for (const auto& vmeth: ifacee.virtual_methods) {
					  if (virtual_methods.contains(vmeth.name))
						  throw DuplicatedVirtualMethodError(type, vmeth.name);
					  virtual_methods.put(vmeth.name, vmeth.type);
				  }
			  };
		// Collect all virtual methods that can be implemented by this class.
		std::function<void(const ClassType&)> collect_virtual_methods
			= [&](const ClassType& clazzz) {
				  if (clazzz.extends) {
					  const auto& super_type = *get_type(*clazzz.extends);
					  if (std::holds_alternative<ClassType>(super_type)) {
						  auto super_type_temp = std::get<ClassType>(super_type);
						  collect_virtual_methods(std::get<ClassType>(super_type));
					  }
				  }
				  for (const auto& iface: clazzz.implements) {
					  //@note: We are guaranteed an InterfaceType is contained. This was checked before.
					  const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
					  // TODO: Change that to invoke collect_virtual_methods.
					  collect_virtual_methods_iface(iface_type);
				  }
				  for (const auto& vmeth: clazzz.virtual_methods) {
					  if (virtual_methods.contains(vmeth.name))
						  throw DuplicatedVirtualMethodError(type, vmeth.name);
					  virtual_methods.put(vmeth.name, vmeth.type);
				  }
			  };
		collect_virtual_methods(clazz);

		base::HashMap<base::StrID, base::StrID> implementations;
		for (const auto& impl: clazz.implementations) {
			// Implemented method is not declared as a virtual.
			if (!virtual_methods.contains(impl.name))
				throw InvalidVirtualMethodImplementationError(type, impl.name);
			if (implementations.contains(impl.name))
				throw DuplicatedVirtualMethodImplementationError(type, impl.name);
			implementations.put(impl.name);

			// Check the signatures of implementations.
			TypeOfData vmethod_type = *get_type(virtual_methods[impl.name]);
			TypeOfData impl_type    = *get_type(impl.type);
			if (!std::holds_alternative<FunctionType>(impl_type))
				throw TypeIsNotFunctionalError(impl.type);
			if (!std::holds_alternative<FunctionType>(vmethod_type))
				throw TypeIsNotFunctionalError(virtual_methods[impl.name]);

			FunctionType implementation = std::get<FunctionType>(impl_type);
			FunctionType vmethod        = std::get<FunctionType>(vmethod_type);

			// First argument should always be a this*
			validate_first_argument(clazz, implementation);
			// Arguments and result types have to match the virtual methods type.
			validate_signature_match(clazz, vmethod, implementation);
		}
	};

	auto validate_implementations_iface = [&](const InterfaceType& interface) {
		// Validates if declared implementations have corresponding virtual method declarations.
		// Validates signatures of implementations.
		base::HashMap<base::StrID, base::StrID> virtual_methods;

		// Collect all virtual methods that can be implemented by this class.
		std::function<void(const InterfaceType&)> collect_virtual_methods_iface
			= [&](const InterfaceType& ifacee) {
				  for (const auto& iface: ifacee.implements) {
					  //@note: We are guaranteed an InterfaceType is contained. This was checked before.
					  const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
					  // TODO: Change that to invoke collect_virtual_methods.
					  collect_virtual_methods_iface(iface_type);
				  }
				  for (const auto& vmeth: ifacee.virtual_methods) {
					  if (virtual_methods.contains(vmeth.name))
						  throw DuplicatedVirtualMethodError(type, vmeth.name);
					  virtual_methods.put(vmeth.name, vmeth.type);
				  }
			  };
		collect_virtual_methods_iface(interface);

		base::HashMap<base::StrID, base::StrID> implementations;
		for (const auto& impl: interface.vmethods_implementations) {
			// Implemented method is not declared as a virtual.
			if (!virtual_methods.contains(impl.name))
				throw InvalidVirtualMethodImplementationError(type, impl.name);
			if (implementations.contains(impl.name))
				throw DuplicatedVirtualMethodImplementationError(type, impl.name);
			implementations.put(impl.name);

			// Check the signatures of implementations.
			TypeOfData vmethod_type = *get_type(virtual_methods[impl.name]);
			TypeOfData impl_type    = *get_type(impl.type);
			if (!std::holds_alternative<FunctionType>(impl_type))
				throw TypeIsNotFunctionalError(impl.type);
			if (!std::holds_alternative<FunctionType>(vmethod_type))
				throw TypeIsNotFunctionalError(virtual_methods[impl.name]);

			FunctionType implementation = std::get<FunctionType>(impl_type);
			FunctionType vmethod        = std::get<FunctionType>(vmethod_type);

			// First argument should always be a this*
			validate_first_argument_iface(interface, implementation);
			// Arguments and result types have to match the virtual methods type.
			validate_signature_match_iface(interface, vmethod, implementation);
		}
	};

	auto validate_all_methods_implemented = [&](const ClassType& clazz) {
		// Collect all methods on the inheritance path and verify they are implemented.
		base::HashMap<base::StrID, base::StrID> virtual_methods;

		// Collect all virtual methods that can be implemented by this class.
		std::function<void(const InterfaceType&)> collect_virtual_methods_iface
			= [&](const InterfaceType& ifacee) {
				  for (const auto& iface: ifacee.implements) {
					  //@note: We are guaranteed an InterfaceType is contained. This was checked before.
					  const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
					  // TODO: Change that to invoke collect_virtual_methods.
					  collect_virtual_methods_iface(iface_type);
				  }
				  for (const auto& vmeth: ifacee.virtual_methods) {
					  if (virtual_methods.contains(vmeth.name))
						  throw DuplicatedVirtualMethodError(type, vmeth.name);
					  virtual_methods.put(vmeth.name, vmeth.type);
				  }
			  };
		// Collect all virtual methods that can and should be implemented by this class.
		std::function<void(const ClassType&)> collect_virtual_methods
			= [&](const ClassType& clazzz) {
				  if (clazzz.extends) {
					  const auto& super_type = *get_type(*clazzz.extends);
					  if (std::holds_alternative<ClassType>(super_type)) {
						  auto super_type_temp = std::get<ClassType>(super_type);
						  collect_virtual_methods(std::get<ClassType>(super_type));
					  }
				  }
				  for (const auto& iface: clazzz.implements) {
					  //@note: We are guaranteed an InterfaceType is contained. This was checked before.
					  const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
					  // TODO: Change that to invoke collect_virtual_methods.
					  collect_virtual_methods_iface(iface_type);
				  }
				  for (const auto& vmeth: clazzz.virtual_methods) {
					  if (virtual_methods.contains(vmeth.name))
						  throw DuplicatedVirtualMethodError(type, vmeth.name);
					  virtual_methods.put(vmeth.name, vmeth.type);
				  }
			  };
		collect_virtual_methods(clazz);

		std::function<void(const ClassType&)> verify_implemented = [&](const ClassType& clazzz) {
			// Remove those implemented by this class.
			for (const auto& impl: clazzz.implementations) virtual_methods.erase(impl.name);

			// Remove those implemented by interfaces.
			for (const auto& iface: clazzz.implements) {
				//@note: We are guaranteed an InterfaceType is contained. This was
				// checked before.
				const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
				for (const auto& impl: iface_type.vmethods_implementations) {
					// TODO: Make this invoke a template verify implemented.
					virtual_methods.erase(impl.name);
				}
			}

			if (clazzz.extends) {
				const auto& super_type = *get_type(*clazzz.extends);
				if (std::holds_alternative<ClassType>(super_type)) {
					auto super_type_temp = std::get<ClassType>(super_type);
					verify_implemented(std::get<ClassType>(super_type));
				}
			}
		};
		verify_implemented(clazz);

		if (!virtual_methods.empty())
			throw UnimplementedVirtualMethodError(type, virtual_methods.begin()->first);
	};

	auto validate_field_duplicates = [&](const ClassType& clazz) {
		// Validates field duplicates on the whole inheritance path.
		base::HashMap<base::StrID, base::StrID> field_definitions;
		std::function<void(const ClassType&)>   collect_fields = [&](const ClassType& clazzz) {
            if (clazzz.extends) {
                const auto& super_type = *get_type(*clazzz.extends);
                if (std::holds_alternative<ClassType>(super_type)) {
                    auto super_type_temp = std::get<ClassType>(super_type);
                    collect_fields(std::get<ClassType>(super_type));
                }
            }

            for (const Field& field: clazzz.fields) {
                if (field_definitions.contains(field.name))
                    throw DuplicatedFieldError(type, field.name);
                field_definitions.put(field.name, field.type);
            }
		};
		collect_fields(clazz);
	};

	auto validate_field_duplicates_data = [&](const DataType& data) {
		base::HashMap<base::StrID, base::StrID> field_definitions;
		for (const auto& field: data.fields) {
			if (field_definitions.contains(field.name))
				throw DuplicatedFieldError(type, field.name);
			field_definitions.put(field.name);
		}
	};

	auto validate_vmethod_signatures = [&](const ClassType& clazzz) {
		for (const auto& vmethod: clazzz.virtual_methods) {
			TypeOfData vmethod_type = *get_type(vmethod.type);
			if (!std::holds_alternative<FunctionType>(vmethod_type))
				throw TypeIsNotFunctionalError(vmethod.type);

			FunctionType vmethod_func = std::get<FunctionType>(vmethod_type);
			validate_first_argument(clazzz, vmethod_func);
		}
	};

	auto validate_vmethod_signatures_interface = [&](const InterfaceType& iface) {
		for (const auto& vmethod: iface.virtual_methods) {
			TypeOfData vmethod_type = *get_type(vmethod.type);
			if (!std::holds_alternative<FunctionType>(vmethod_type))
				throw TypeIsNotFunctionalError(vmethod.type);

			FunctionType vmethod_func = std::get<FunctionType>(vmethod_type);
			validate_first_argument_iface(iface, vmethod_func);
		}
	};

	auto validate_extends = [&](const ClassType& clazzz) {
		if_opt_some(clazzz.extends, extends) {
			auto super_type = get_type(extends);
			if (!std::holds_alternative<ClassType>(*super_type) || *super_type == type)
				throw InvalidExtends(type);
		}
	};

	variant_match(type) {
		// Think about how other types should be verified.
		// TODO: Funkcje ktorym przekażemy typ funkcyjny jako argument?
		variant_case(VariantType, variant) {
			// @todo: Verify empty variants.
		}
		variant_case(DataType, data) { validate_field_duplicates_data(data); }
		variant_case(InterfaceType, interface) {
			validate_implements(interface.implements);
			validate_vmethod_signatures_interface(interface);
			validate_implementations_iface(interface);
		}
		variant_case(ClassType, clazz) {
			validate_extends(clazz);
			validate_field_duplicates(clazz);
			validate_implements(clazz.implements);
			validate_vmethod_signatures(clazz);
			validate_implementations(clazz);
			if (!clazz.is_abstract) validate_all_methods_implemented(clazz);
		}
	}
}

void TypeContextBuilder::validateTypes() const {
	// Check for cycles in hierarchy.
	enum Status { Waiting, Visited, Done };

	base::HashMap<base::StrID, Status> status;
	for (const auto& type: types) status.put(typeName(type), Waiting);

	// explicit object parameter lambdas don't seem to work with class members, hence the
	// reference
	auto& types_ref = types;
	auto  helper    = [&](this auto self, const auto& type) {
        auto name = typeName(type);
        if (status[name] == Visited)
            throw CycleInHierarchyError(type);
        else if (status[name] == Done)
            return;

        status[name] = Visited;
        variant_match(type) {
            variant_case(ClassType, clazz) {
                if_opt_some(clazz.extends, superclass) self(*types_ref.at(superclass));
                for (auto iface: clazz.implements) self(*types_ref.at(iface));
            }
            variant_case(InterfaceType, interface) {
                for (auto iface: interface.implements) self(*types_ref.at(iface));
            }
        }
        status[name] = Done;
	};
	for (const auto& type: types) helper(type);

	// @note: This validation assumes cycles in class hierarchy where detected.
	for (const auto& type: types) validateType(type);
}

TypeContext TypeContextBuilder::build() const {
	validateTypes();
	TypeContext tctx;
	tctx.types = types;
	for (const auto& type: types)
		tctx.metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));
	auto to_low_type
		= [&](const TypeOfData& tod) { return tctx.metadata->at(VISIT(tod, tp, return tp.name)); };
	for (const auto& type: tctx.types) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				tctx.metadata->at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				tctx.metadata->at(data.name)->definePointer(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::StaticTableType, data) {
				tctx.metadata->at(data.name)->defineStaticTable(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner),
					data.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, data) {
				tctx.metadata->at(data.name)->defineDynamicTable(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						tctx.metadata->atMaybe(field.type)
							.expect<UnknownSubtypeError>(data, field.name)
					);
				tctx.metadata->at(data.name)->defineData(fields, {});
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(
						tctx.metadata->atMaybe(variant).expect<UnknownSubtypeError>(data, variant)
					);
				tctx.metadata->at(data.name)->defineVariant(variants);
			}
			variant_case(vm::code::FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters)
					parameters.emplace_back(tctx.metadata->at(param));
				tctx.metadata->at(data.name)->defineFunction(
					parameters,
					tctx.metadata->atMaybe(data.result).expect<UnknownSubtypeError>(data, data.result)
				);
			}
			variant_case(vm::code::OpaqueType, opaque) {
				tctx.metadata->at(opaque.name)->defineOpaque(opaque.size);
			}
			variant_case(vm::code::ClassType, data) {
				auto get_type = [&](base::StrID name) {
					return types.atMaybe(name).expect<UnknownSubtypeError>(type, name);
				};

				// @note: First field in the data types is always a vt.
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};

				// Collects all fields from superclasses and insert them into the vector.
				std::function<void(const ClassType&)> collect_fields = [&](const ClassType& clazz) {
					if (clazz.extends) {
						const auto& super_type = *get_type(*clazz.extends);
						if (std::holds_alternative<ClassType>(super_type)) {
							auto super_type_temp = std::get<ClassType>(super_type);
							collect_fields(std::get<ClassType>(super_type));
						}
					}

					// TODO: At maybes may not be needed?
					for (const Field& field: clazz.fields) {
						fields.emplace_back(
							field.name,
							tctx.metadata->atMaybe(field.type)
								.expect<UnknownSubtypeError>(data, field.name)
						);
					}
				};
				collect_fields(data);

				TypeRef tp            = tctx.metadata->at(data.name);
				auto    get_type_cref = [&](base::StrID name) -> TypeCRef {
                    return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};

				auto kind = InheritanceMetadata::Class{
					.is_abstract = data.is_abstract,
					.extends     = data.extends.map(get_type_cref),
				};
				auto implements = data.implements | std::views::transform(get_type_cref)
				                | std::ranges::to<std::vector>();

				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type_cref(method.type));

				base::HashMap<base::StrID, TypeCRef> implementations;
				for (auto& method: data.implementations)
					implementations.put(method.name, get_type_cref(method.type));

				// TODO: How is it possible that we double verify the code xD?
				// Construct a vtable.
				base::HashMap<base::StrID, TypeCRef>  vtable;
				std::function<void(const ClassType&)> create_vtable = [&](const ClassType& clazz) {
					// TODO: At maybes may not be needed?
					for (const auto& impl: clazz.implementations)
						if (!vtable.contains(impl.name))
							vtable.put(impl.name, get_type_cref(impl.type));

					for (const auto& iface: clazz.implements) {
						//@note: We are guaranteed an InterfaceType is contained. This was checked
						// before.
						const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
						// TODO: Change that to invoke collect_virtual_methods.
						for (const auto& impl: iface_type.vmethods_implementations)
							vtable.put(impl.name, get_type_cref(impl.type));
					}

					if (clazz.extends) {
						const auto& super_type = *get_type(*clazz.extends);
						if (std::holds_alternative<ClassType>(super_type)) {
							auto super_type_temp = std::get<ClassType>(super_type);
							create_vtable(std::get<ClassType>(super_type));
						}
					}
				};
				create_vtable(data);

				std::cerr << "=========== CREATING CLASS ==============\n";
				std::cerr << "Declared fields: \n";
				for (const auto& x: fields) std::cerr << x.first.strView() << " | ";
				std::cerr << "\nDeclared vmethods: \n";
				for (const auto& x: virtual_methods) std::cerr << x.first.strView() << " | ";
				std::cerr << "\nVTable: \n";
				for (const auto& x: vtable)
					std::cerr << x.first.strView() << ", " << x.second->getName().strView()
							  << " | ";
				std::cerr << "\n==========================================\n";

				vm::InheritanceMetadata inheritance_metadata(
					tp,
					kind,
					std::move(implements),
					std::move(virtual_methods),
					std::move(implementations),
					std::move(vtable)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_case(vm::code::InterfaceType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				TypeRef tp       = tctx.metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();

				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type(method.type));

				base::HashMap<base::StrID, TypeCRef> implementations;
				for (auto& method: data.vmethods_implementations)
					implementations.put(method.name, get_type(method.type));

				// TODO: VTable building should appear here.

				vm::InheritanceMetadata inheritance_metadata(
					tp,
					InheritanceMetadata::Interface{},
					std::move(implements),
					std::move(virtual_methods),
					std::move(implementations)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	tctx.metadata->finalize();
	return tctx;
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& TypeContext::getTypes() const { return types; }

const vm::TypeMetadata& TypeContext::getMetadata() const { return *metadata; }

Box<vm::TypeMetadata> TypeContext::moveMetadata() && { return std::move(metadata); }

void TypeContextBuilder::addType(const TypeOfData& type) {
	const auto name = VISIT(type, tp, return tp.name);
	match_optional(types.atMaybe(name)) {
		opt_some(tp) {
			if (type != *tp) throw DuplicatedTypeError(type);
		}
		opt_none { types.insert(type, name); }
	}
}
