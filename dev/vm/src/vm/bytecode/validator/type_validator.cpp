#include "type_validator.hpp"

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

template<typename ExpectedType, typename ErrorFactory>
const ExpectedType& vm::code::TypeContext::getType(
	base::StrID name, const TypeOfData& context, ErrorFactory error_factory
) const {
	const TypeOfData& type_of_data = *types.atMaybe(name).expect<UnknownSubtypeError>(context, name);
	if (const auto* specific_type = std::get_if<ExpectedType>(&type_of_data)) return *specific_type;
	throw error_factory();
}

template<typename FieldableType>
void vm::code::TypeContext::collectFieldsRecursive(
	const FieldableType&                     fieldable,
	const TypeOfData&                        error_context,
	base::HashMap<base::StrID, base::StrID>& fields
) const {
	if constexpr (std::is_same_v<FieldableType, ClassType>) {
		if (fieldable.extends) {
			const auto& super_class = getType<ClassType>(*fieldable.extends, error_context, [&]() {
				return InvalidExtendsError(fieldable, *fieldable.extends);
			});
			collectFieldsRecursive(super_class, error_context, fields);
		}
	}
	for (const auto& field_type: fieldable.fields) {
		if (fields.contains(field_type.name))
			throw DuplicatedFieldError(error_context, field_type.name);
		fields.put(field_type.name, field_type.type);
	}
}

template<typename InheritableType, typename ErrorContextType>
void vm::code::TypeContext::collectVirtualMethodsRecursive(
	const InheritableType&                   inh,
	const ErrorContextType&                  error_context_inh,
	base::HashMap<base::StrID, base::StrID>& virtual_methods
) const {
	for (const auto& interface_name: inh.implements) {
		const auto& interface = getType<InterfaceType>(interface_name, inh, [&]() {
			return InvalidImplementsError(inh, interface_name);
		});
		collectVirtualMethodsRecursive(interface, error_context_inh, virtual_methods);
	}

	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		if (inh.extends) {
			const auto& super_class = getType<ClassType>(*inh.extends, error_context_inh, [&]() {
				return InvalidExtendsError(inh, *inh.extends);
			});
			collectVirtualMethodsRecursive(super_class, error_context_inh, virtual_methods);
		}
	}
	for (const auto& vmethod: inh.virtual_methods) {
		if (virtual_methods.contains(vmethod.name))
			throw DuplicatedVirtualMethodError(error_context_inh, vmethod.name);
		virtual_methods.put(vmethod.name, vmethod.type);
	}
}

template<typename InheritableType, typename ErrorContextType>
void vm::code::TypeContext::collectImplementationsRecursive(
	const InheritableType&                   inh,
	const ErrorContextType&                  error_context_inh,
	base::HashMap<base::StrID, base::StrID>& implementations
) const {
	for (const auto& impl: inh.implementations) implementations.put(impl.name, impl.type);
	for (const auto& interface_name: inh.implements) {
		const auto& interface = getType<InterfaceType>(interface_name, error_context_inh, [&]() {
			return InvalidImplementsError(inh, interface_name);
		});
		collectImplementationsRecursive(interface, error_context_inh, implementations);
	}
	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		if (inh.extends) {
			const auto& super_class = getType<ClassType>(*inh.extends, error_context_inh, [&]() {
				return InvalidExtendsError(inh, *inh.extends);
			});
			collectImplementationsRecursive(super_class, error_context_inh, implementations);
		}
	}
}

template<typename InheritableType>
void vm::code::TypeContext::validateMethodFirstArgument(
	const InheritableType& inh, const FunctionType& func_type
) const {
	if (func_type.parameters.empty()) throw MethodFirstArgumentError(inh, func_type.name);
	const auto& first_param_type_name = func_type.parameters[0];
	const auto& first_param_type      = getType<PointerType>(first_param_type_name, inh, [&]() {
        return MethodFirstArgumentError(inh, func_type.name);
    });
	if (first_param_type.inner != inh.name) throw MethodFirstArgumentError(inh, func_type.name);
}

template<typename InheritableType>
void vm::code::TypeContext::validateMethodSignatureMatch(
	const InheritableType& inh, const FunctionType& vmethod_type, const FunctionType& impl_type
) const {
	if (vmethod_type.result != impl_type.result) throw MethodTypeError(inh, impl_type.name);
	if (vmethod_type.parameters.size() != impl_type.parameters.size())
		throw MethodTypeError(inh, impl_type.name);
	for (u64 i = 1; i < impl_type.parameters.size(); i++)
		if (vmethod_type.parameters[i] != impl_type.parameters[i])
			throw MethodTypeError(inh, impl_type.name);
}

template<typename InheritableType>
void vm::code::TypeContext::validateVMethodSignatures(const InheritableType& inh) const {
	for (const auto& vmethod: inh.virtual_methods) {
		const auto& vmethod_type = getType<FunctionType>(vmethod.type, inh, [&]() {
			return TypeIsNotFunctionalError(vmethod.type);
		});
		validateMethodFirstArgument(inh, vmethod_type);
	}
}

template<typename InheritableType>
void vm::code::TypeContext::validateImplementations(
	const InheritableType& inh, const base::HashMap<base::StrID, base::StrID>& virtual_methods
) const {
	base::HashMap<base::StrID, base::StrID> implementations;
	for (const auto& implementation: inh.implementations) {
		if (implementations.contains(implementation.name))
			throw DuplicatedVirtualMethodImplementationError(inh, implementation.name);
		implementations.put(implementation.name);

		if (!virtual_methods.contains(implementation.name))
			throw InvalidVirtualMethodImplementationError(inh, implementation.name);

		const auto& vmethod_name   = virtual_methods[implementation.name];
		const auto& impl_type_name = implementation.type;
		const auto& vmethod_type   = getType<FunctionType>(vmethod_name, inh, [&]() {
            return TypeIsNotFunctionalError(vmethod_name);
        });
		const auto& impl_type      = getType<FunctionType>(impl_type_name, inh, [&]() {
            return TypeIsNotFunctionalError(impl_type_name);
        });

		validateMethodFirstArgument(inh, impl_type);
		validateMethodSignatureMatch(inh, vmethod_type, impl_type);
	}
}

void vm::code::TypeContext::validateAllMethodsImplemented(
	const ClassType& clazz, const base::HashMap<base::StrID, base::StrID>& virtual_methods
) const {
	base::HashMap<base::StrID, base::StrID> implementations;
	collectImplementationsRecursive(clazz, clazz, implementations);

	for (auto& [name, _]: virtual_methods)
		if (!implementations.contains(name)) throw UnimplementedVirtualMethodError(clazz, name);
}

template<typename FieldableType>
void vm::code::TypeContext::validateFieldDuplicates(const FieldableType& fieldable) const {
	base::HashMap<base::StrID, base::StrID> fields;
	collectFieldsRecursive(fieldable, fieldable, fields);
}

template<typename InheritableType>
void vm::code::TypeContext::validateImplementsDuplicates(const InheritableType& inh) const {
	base::HashMap<base::StrID, base::StrID> interfaces;
	for (const auto& impl: inh.implements) {
		if (interfaces.contains(impl)) throw DuplicatedImplementsError(inh, impl);
		interfaces.put(impl);
	}
}

template<typename InheritableType, typename ErrorContextType>
void vm::code::TypeContext::buildVTableRecursive(
	const InheritableType&                inh,
	const ErrorContextType&               error_context_inh,
	base::HashMap<base::StrID, TypeCRef>& vtable,
	TypeMetadata&                         metadata
) const {
	auto get_type_cref = [&](base::StrID name) -> TypeCRef {
		return metadata.atMaybe(name).expect<UnknownSubtypeError>(inh, name);
	};

	for (const auto& impl: inh.implementations) vtable.put(impl.name, get_type_cref(impl.type));
	for (const auto& interface_name: inh.implements) {
		const auto& interface = getType<InterfaceType>(interface_name, error_context_inh, [&]() {
			return InvalidImplementsError(error_context_inh, interface_name);
		});
		buildVTableRecursive(interface, error_context_inh, vtable, metadata);
	}
	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		if (inh.extends) {
			const auto& super_class = getType<ClassType>(*inh.extends, error_context_inh, [&]() {
				return InvalidExtendsError(inh, *inh.extends);
			});
			buildVTableRecursive(super_class, error_context_inh, vtable, metadata);
		}
	}
}

template<typename InheritableType>
vm::code::TypeContext::FieldVector vm::code::TypeContext::buildFieldVector(
	const InheritableType& inh, TypeMetadata& metadata
) const {
	auto to_low_type
		= [&](const TypeOfData& tod) { return metadata.at(VISIT(tod, type, return type.name)); };
	FieldVector fields{ { base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) } };

	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		std::function<void(const vm::code::ClassType&)> collect_class_fields_recursive
			= [&](const vm::code::ClassType& clazz) {
				  if (clazz.extends) {
					  const auto& super_class_code
						  = getType<vm::code::ClassType>(*clazz.extends, inh, [&]() {
								return InvalidExtendsError(inh, *clazz.extends);
							});
					  collect_class_fields_recursive(super_class_code);
				  }

				  for (const vm::code::Field& field_code: clazz.fields) {
					  fields.emplace_back(
						  field_code.name,
						  metadata.atMaybe(field_code.type)
							  .expect<UnknownSubtypeError>(inh, field_code.name)
					  );
				  }
			  };
		collect_class_fields_recursive(inh);
	}

	return fields;
}

template<typename InheritableType>
vm::InheritanceMetadata vm::code::TypeContext::buildInheritanceMetadata(
	const InheritableType& inh, TypeMetadata& metadata
) const {
	TypeCRef tp            = metadata.at(inh.name);
	auto     get_type_cref = [&](base::StrID name) -> TypeCRef {
        return metadata.atMaybe(name).expect<UnknownSubtypeError>(inh, name);
	};
	auto implements
		= inh.implements | std::views::transform(get_type_cref) | std::ranges::to<std::vector>();

	base::HashMap<base::StrID, TypeCRef> virtual_methods;
	for (auto& method: inh.virtual_methods)
		virtual_methods.put(method.name, get_type_cref(method.type));

	base::HashMap<base::StrID, TypeCRef> implementations;
	for (auto& method: inh.implementations)
		implementations.put(method.name, get_type_cref(method.type));

	base::HashMap<base::StrID, TypeCRef> vtable;
	buildVTableRecursive(inh, inh, vtable, metadata);

	vm::InheritanceMetadata::Kind kind;
	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		kind = InheritanceMetadata::Class{
			.is_abstract = inh.is_abstract,
			.extends     = inh.extends.map(get_type_cref),
		};
	} else {
		kind = vm::InheritanceMetadata::Interface{};
	}

	return {
		tp, kind, std::move(implements), std::move(virtual_methods), std::move(vtable),
	};
}

void vm::code::TypeContext::validateType(const TypeOfData& type) const {
	variant_match(type) {
		variant_case(VariantType, variant) {
			if (variant.variant_alternatives.empty()) throw EmptyVariantError(variant);
		}
		variant_case(DataType, data) { validateFieldDuplicates(data); }
		variant_case(InterfaceType, interface) {
			base::HashMap<base::StrID, base::StrID> virtual_methods;
			collectVirtualMethodsRecursive(interface, interface, virtual_methods);

			validateImplementsDuplicates(interface);
			validateVMethodSignatures(interface);
			validateImplementations(interface, virtual_methods);
		}
		variant_case(ClassType, clazz) {
			// All virtual methods that can be implemented by this class(including superclass and
			// interface vmethods as well).
			base::HashMap<base::StrID, base::StrID> virtual_methods;
			collectVirtualMethodsRecursive(clazz, clazz, virtual_methods);

			validateFieldDuplicates(clazz);
			validateImplementsDuplicates(clazz);
			validateVMethodSignatures(clazz);
			validateImplementations(clazz, virtual_methods);
			if (!clazz.is_abstract) validateAllMethodsImplemented(clazz, virtual_methods);
		}
	}
}

void vm::code::TypeContext::validateTypes() const {
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
                if_opt_some(clazz.extends, superclass) {
                    if (!types_ref.contains(superclass))
                        throw InvalidExtendsError(clazz, superclass);
                    self(*types_ref.at(superclass));
                }
                for (auto iface: clazz.implements) {
                    if (!types_ref.contains(iface)) throw InvalidImplementsError(clazz, iface);
                    self(*types_ref.at(iface));
                }
            }
            variant_case(InterfaceType, interface) {
                for (auto iface: interface.implements) {
                    if (!types_ref.contains(iface)) throw InvalidImplementsError(interface, iface);
                    self(*types_ref.at(iface));
                }
            }
        }
        status[name] = Done;
	};
	for (const auto& type: types) helper(type);

	// @note: Following validation assumes cycles in class hierarchy where detected.
	for (const auto& type: types) validateType(type);
}

Box<vm::TypeMetadata> vm::code::TypeContext::validateAndProduceTypeMetadata() const {
	validateTypes();
	Box<TypeMetadata> metadata = makeBox<TypeMetadata>();

	// Declare all types first
	for (const auto& type: types) metadata->addType(Type::declareType(typeName(type)));

	// Well-define every type.
	for (const auto& type: types) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				metadata->at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				metadata->at(data.name)->definePointer(
					metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::StaticTableType, data) {
				metadata->at(data.name)->defineStaticTable(
					metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner),
					data.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, data) {
				metadata->at(data.name)->defineDynamicTable(
					metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::DataType, data) {
				FieldVector fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						metadata->atMaybe(field.type).expect<UnknownSubtypeError>(data, field.name)
					);
				metadata->at(data.name)->defineData(fields, {});
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(
						metadata->atMaybe(variant).expect<UnknownSubtypeError>(data, variant)
					);
				metadata->at(data.name)->defineVariant(variants);
			}
			variant_case(vm::code::FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters) parameters.emplace_back(metadata->at(param));
				metadata->at(data.name)->defineFunction(
					parameters,
					metadata->atMaybe(data.result).expect<UnknownSubtypeError>(data, data.result)
				);
			}
			variant_case(vm::code::OpaqueType, opaque) {
				metadata->at(opaque.name)->defineOpaque(opaque.size);
			}
			variant_case(vm::code::ClassType, clazz) {
				TypeRef                 tp     = metadata->at(clazz.name);
				FieldVector             fields = buildFieldVector(clazz, *metadata.refMut());
				vm::InheritanceMetadata inh_metadata
					= buildInheritanceMetadata(clazz, *metadata.refMut());
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_case(vm::code::InterfaceType, interface) {
				TypeRef                 tp     = metadata->at(interface.name);
				FieldVector             fields = buildFieldVector(interface, *metadata.refMut());
				vm::InheritanceMetadata inh_metadata
					= buildInheritanceMetadata(interface, *metadata.refMut());
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	metadata->finalize();
	return metadata;
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& vm::code::TypeContext::getCurrentTypes() const {
	return types;
}

void vm::code::TypeContext::insertType(const TypeOfData& type) {
	const auto name = typeName(type);
	match_optional(types.atMaybe(name)) {
		opt_some(previous_type) {
			if (type != *previous_type) throw DuplicatedTypeError(type, *previous_type);
		}
		opt_none { types.insert(type, name); }
	}
}
