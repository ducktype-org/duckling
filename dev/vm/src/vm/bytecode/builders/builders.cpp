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
#include "vm/core/process/type_metadata/inheritance_metadata.hpp"
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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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

const vm::code::TypeOfData& TypeContextBuilder::getTypeOfData(
	base::StrID name, const TypeOfData& context
) const {
	return *types.atMaybe(name).expect<UnknownSubtypeError>(context, name);
}

template<typename ExpectedType, typename ErrorFactory>
const ExpectedType& TypeContextBuilder::getType(
	base::StrID name, const TypeOfData& context, ErrorFactory error_factory
) const {
	const TypeOfData& type_of_data = getTypeOfData(name, context);
	if (const auto* specific_type = std::get_if<ExpectedType>(&type_of_data)) return *specific_type;
	throw error_factory();
}

// Class or interface.
template<typename InheritableType>
void TypeContextBuilder::validateMethodFirstArgument(
	const InheritableType& inh, const FunctionType& func_type
) const {
	if (func_type.parameters.empty()) throw MethodFirstArgumentError(inh, func_type.name);
	const auto& first_param_type_name = func_type.parameters[0];
	const auto& first_param_type      = getType<PointerType>(first_param_type_name, inh, [&]() {
        return MethodFirstArgumentError(inh, func_type.name);
    });
	if (first_param_type.inner != inh.name) throw MethodFirstArgumentError(inh, func_type.name);
}

// Class or interface.
template<typename InheritableType>
void TypeContextBuilder::validateMethodSignatureMatch(
	const InheritableType& inh, const FunctionType& vmethod_type, const FunctionType& impl_type
) const {
	if (vmethod_type.result != impl_type.result) throw MethodTypeError(inh, impl_type.name);
	if (vmethod_type.parameters.size() != impl_type.parameters.size())
		throw MethodTypeError(inh, impl_type.name);
	for (u64 i = 1; i < impl_type.parameters.size(); i++)
		if (vmethod_type.parameters[i] != impl_type.parameters[i])
			throw MethodTypeError(inh, impl_type.name);
}

// Class of Interface.
template<typename InheritableType, typename ErrorContextType>
void TypeContextBuilder::collectVirtualMethodsRecursive(
	const InheritableType&                   inh,
	const ErrorContextType&                  error_context_inh,
	base::HashMap<base::StrID, base::StrID>& virtual_methods,
	bool                                     throw_on_duplicates  // TODO: That may be not needed.
) const {
	for (const auto& interface_name: inh.implements) {
		// TODO: Potentially move implements validation here.
		const auto& interface = getType<InterfaceType>(interface_name, inh, [&]() {
			return InvalidImplementsError(error_context_inh);
		});
		collectVirtualMethodsRecursive<InterfaceType>(interface, error_context_inh, virtual_methods);
	}

	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		if (inh.extends) {
			const auto& super_class = getType<ClassType>(*inh.extends, error_context_inh, [&]() {
				return InvalidExtends(inh);
			});
			collectVirtualMethodsRecursive<ClassType>(
				super_class, error_context_inh, virtual_methods
			);
		}
	}
	for (const auto& vmethod: inh.virtual_methods) {
		if (throw_on_duplicates && virtual_methods.contains(vmethod.name))
			throw DuplicatedVirtualMethodError(error_context_inh, vmethod.name);
		virtual_methods.put(vmethod.name, vmethod.type);
	}
}

// Class or Data.
template<typename FieldableType>
void TypeContextBuilder::collectFieldsRecursive(
	const FieldableType&                     fieldable,
	const TypeOfData&                        error_context,
	base::HashMap<base::StrID, base::StrID>& fields
) const {
	if constexpr (std::is_same_v<FieldableType, ClassType>) {
		if (fieldable.extends) {
			const auto& super_class = getType<ClassType>(*fieldable.extends, error_context, [&]() {
				return InvalidExtends(fieldable);
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

template<typename InheritableType>
void TypeContextBuilder::validateImplementations(const InheritableType& inh) const {
	// TODO: Maybe it makes no sense to create a new map every time.
	base::HashMap<base::StrID, base::StrID> virtual_methods;
	collectVirtualMethodsRecursive(inh, inh, virtual_methods);

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
		// First argument should always be a this*
		validateMethodFirstArgument(inh, impl_type);
		// Arguments and result types have to match the virtual methods type.
		validateMethodSignatureMatch(inh, vmethod_type, impl_type);
	}
}

template<typename InheritableType, typename ErrorContextType>
void TypeContextBuilder::collectImplementationsRecursive(
	const InheritableType&                   inh,
	const ErrorContextType&                  error_context_inh,
	base::HashMap<base::StrID, base::StrID>& implementations
) const {
	// Insert those implemented by this class.
	for (const auto& impl: inh.implementations) implementations.put(impl.name, impl.type);
	for (const auto& interface_name: inh.implements) {
		const auto& interface = getType<InterfaceType>(interface_name, error_context_inh, [&]() {
			return InvalidImplementsError(error_context_inh);
		});
		collectImplementationsRecursive(interface, error_context_inh, implementations);
	}
	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		if (inh.extends) {
			const auto& super_class = getType<ClassType>(*inh.extends, error_context_inh, [&]() {
				return InvalidExtends(inh);
			});
			collectImplementationsRecursive(super_class, error_context_inh, implementations);
		}
	}
}

template<typename InheritableType>
void TypeContextBuilder::validateVMethodSignatures(const InheritableType& inh) const {
	for (const auto& vmethod: inh.virtual_methods) {
		const FunctionType& vmethod_type = getType<FunctionType>(vmethod.type, inh, [&]() {
			return TypeIsNotFunctionalError(vmethod.type);
		});
		validateMethodFirstArgument(inh, vmethod_type);
	}
}

// Potentially pass the required vmethods map as a parameter so it's faster.
void TypeContextBuilder::validateAllMethodsImplemented(const ClassType& clazz) const {
	base::HashMap<base::StrID, base::StrID> virtual_methods;
	collectVirtualMethodsRecursive(clazz, clazz, virtual_methods);

	base::HashMap<base::StrID, base::StrID> implementations;
	collectImplementationsRecursive(clazz, clazz, implementations);

	for (auto& [name, _]: virtual_methods)
		if (!implementations.contains(name)) throw UnimplementedVirtualMethodError(clazz, name);
}

template<typename FieldableType>
void TypeContextBuilder::validateFieldDuplicates(const FieldableType& fieldable) const {
	base::HashMap<base::StrID, base::StrID> fields;
	collectFieldsRecursive(fieldable, fieldable, fields);
}

template<typename InheritableType>
void TypeContextBuilder::validateImplementsDuplicates(const InheritableType& inh) const {
	base::HashMap<base::StrID, base::StrID> interfaces;
	for (const auto& impl: inh.implements) {
		if (interfaces.contains(impl)) throw DuplicatedImplementsError(inh);
		interfaces.put(impl);
	}
}

template<typename InheritableType, typename ErrorContextType>
void TypeContextBuilder::buildVTableRecursive(
	const InheritableType&                inh,
	const ErrorContextType&               error_context_inh,
	base::HashMap<base::StrID, TypeCRef>& vtable,
	const TypeContext&                    tctx
) const {
	auto get_type_cref = [&](base::StrID name) -> TypeCRef {
		return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(inh, name);
	};
	// Insert those implemented by this class.
	for (const auto& impl: inh.implementations) vtable.put(impl.name, get_type_cref(impl.type));

	for (const auto& interface_name: inh.implements) {
		const auto& interface = getType<InterfaceType>(interface_name, error_context_inh, [&]() {
			return InvalidImplementsError(error_context_inh);
		});
		buildVTableRecursive(interface, error_context_inh, vtable, tctx);
	}
	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		if (inh.extends) {
			const auto& super_class = getType<ClassType>(*inh.extends, error_context_inh, [&]() {
				return InvalidExtends(inh);
			});
			buildVTableRecursive(super_class, error_context_inh, vtable, tctx);
		}
	}
}

void TypeContextBuilder::validateType(const TypeOfData& type) const {
	// TODO: What is someone declares a field named vt.
	// TODO: Funkcje ktorym przekażemy typ funkcyjny jako argument?
	// TODO: Think about how other types should be verified.

	variant_match(type) {
		variant_case(VariantType, variant) {
			if (variant.variant_alternatives.empty()) throw EmptyVariantError(variant);
		}
		variant_case(DataType, data) { validateFieldDuplicates(data); }
		variant_case(InterfaceType, interface) {
			validateImplementsDuplicates(interface);
			validateVMethodSignatures(interface);
			validateImplementations(interface);
		}
		variant_case(ClassType, clazz) {
			validateFieldDuplicates(clazz);
			validateImplementsDuplicates(clazz);
			validateVMethodSignatures(clazz);
			validateImplementations(clazz);
			if (!clazz.is_abstract) validateAllMethodsImplemented(clazz);
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

	// @note: Following validation assumes cycles in class hierarchy where detected.
	for (const auto& type: types) validateType(type);
}

template<typename InheritableType>
TypeContextBuilder::FieldVector TypeContextBuilder::buildFieldVector(
	const InheritableType& inh, const TypeContext& tctx
) const {
	auto to_low_type = [&](const TypeOfData& tod) {
		return tctx.metadata->at(VISIT(tod, type, return type.name));
	};
	FieldVector fields{ { base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) } };

	if constexpr (std::is_same_v<InheritableType, ClassType>) {
		std::function<void(const vm::code::ClassType&)> collect_class_fields_recursive
			= [&](const vm::code::ClassType& clazz) {
				  if (clazz.extends) {
					  const auto& super_class_code = getType<vm::code::ClassType>(
						  *clazz.extends, inh, [&]() { return InvalidExtends(inh); }
					  );
					  collect_class_fields_recursive(super_class_code);
				  }

				  for (const vm::code::Field& field_code: clazz.fields) {
					  fields.emplace_back(
						  field_code.name,
						  tctx.metadata->atMaybe(field_code.type)
							  .expect<UnknownSubtypeError>(inh, field_code.name)
					  );
				  }
			  };
		collect_class_fields_recursive(inh);
	}

	return fields;
}

template<typename InheritableType>
vm::InheritanceMetadata TypeContextBuilder::buildInheritanceMetadata(
	const InheritableType& inh, const TypeContext& tctx
) const {
	TypeRef tp            = tctx.metadata->at(inh.name);
	auto    get_type_cref = [&](base::StrID name) -> TypeCRef {
        return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(inh, name);
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
	buildVTableRecursive(inh, inh, vtable, tctx);

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
		tp,
		kind,
		std::move(implements),
		std::move(virtual_methods),
		std::move(implementations),
		std::move(vtable),
	};
}

TypeContext TypeContextBuilder::build() const {
	validateTypes();
	TypeContext tctx;
	tctx.types = types;
	for (const auto& type: types)
		tctx.metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));
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
				FieldVector fields;
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
			variant_case(vm::code::ClassType, clazz) {
				TypeRef                 tp           = tctx.metadata->at(clazz.name);
				FieldVector             fields       = buildFieldVector(clazz, tctx);
				vm::InheritanceMetadata inh_metadata = buildInheritanceMetadata(clazz, tctx);
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_case(vm::code::InterfaceType, interface) {
				TypeRef                 tp           = tctx.metadata->at(interface.name);
				FieldVector             fields       = buildFieldVector(interface, tctx);
				vm::InheritanceMetadata inh_metadata = buildInheritanceMetadata(interface, tctx);
				tp->defineData(fields, std::move(inh_metadata));
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
