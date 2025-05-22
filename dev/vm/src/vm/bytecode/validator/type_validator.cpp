#include "type_validator.hpp"

#include "errors.hpp"

#include <vm/bytecode/builtin_types.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

void vm::code::TypeContext::validateType(const TypeOfData& type) const {
	auto get_type = [&](base::StrID name) {
		return types.atMaybe(name).expect<UnknownSubtypeError>(type, name);
	};
	auto validate_implements = [&](const std::vector<base::StrID>& implements) {
		for (const auto& impl: implements) {
			auto impl_type = get_type(impl);
			if (!std::holds_alternative<InterfaceType>(*impl_type) || *impl_type == type)
				throw InvalidImplementsError(type);
		}
	};

	variant_match(type) {
		variant_case(InterfaceType, interface) { validate_implements(interface.implements); }
		variant_case(ClassType, clazz) {
			validate_implements(clazz.implements);
			if_opt_some(clazz.extends, extends) {
				auto super_type = get_type(extends);
				if (!std::holds_alternative<ClassType>(*super_type) || *super_type == type)
					throw InvalidExtends(type);

				const auto& superclass = std::get<ClassType>(*super_type);
				if (clazz.fields.size() < superclass.fields.size())
					throw MissingAncestorFieldError(type);
				for (auto [field, super_field]: std::views::zip(clazz.fields, superclass.fields))
					if (field != super_field) throw MissingAncestorFieldError(type);
			}
		}
	}
}

void vm::code::TypeContext::validateTypes() const {
	for (const auto& type: types) validateType(type);

	// Check for cycles in hierarchy.
	enum Status { Waiting, Visited, Done };

	base::HashMap<base::StrID, Status> status;
	for (const auto& type: types) status.put(typeName(type), Waiting);

	// explicit object parameter lambdas don't seem to work with class members, hence the reference
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
}

Box<vm::TypeMetadata> vm::code::TypeContext::validateAndProduceTypeMetadata() const {
	validateTypes();
	Box<TypeMetadata> metadata = makeBox<TypeMetadata>();

	// Declare all types first
	for (const auto& type: types)
		metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));

	auto to_low_type
		= [&](const TypeOfData& tod) { return metadata->at(VISIT(tod, tp, return tp.name)); };

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
				std::vector<std::pair<base::StrID, TypeRef>> fields;
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
			variant_case(vm::code::ClassType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				fields.reserve(data.fields.size() + 1);
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						metadata->atMaybe(field.type).expect<UnknownSubtypeError>(data, field.name)
					);
				TypeRef tp       = metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return metadata->atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};
				auto kind = InheritanceMetadata::Class{
					.is_abstract = data.is_abstract,
					.extends     = data.extends.map(get_type),
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();
				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type(method.type));
				vm::InheritanceMetadata inheritance_metadata(
					tp, kind, std::move(implements), std::move(virtual_methods)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_case(vm::code::InterfaceType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				TypeRef tp       = metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return metadata->atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();
				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type(method.type));
				vm::InheritanceMetadata inheritance_metadata(
					tp,
					InheritanceMetadata::Interface{},
					std::move(implements),
					std::move(virtual_methods)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
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
	const auto name = VISIT(type, tp, return tp.name);
	match_optional(types.atMaybe(name)) {
		opt_some(previous_type) {
			if (type != *previous_type) throw DuplicatedTypeError(type, *previous_type);
		}
		opt_none { types.insert(type, name); }
	}
}
