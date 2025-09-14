#include "type_builder.hpp"

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_utils.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace {
	using namespace vm::code::detail;

	/**
	 * @brief Recursively builds a vtable layout for an InheritableType.
	 */
	template<InheritableTypeConcept InheritableType, TypeOfDataConcept ErrorContextType>
	void buildVTableRecursive(
		const InheritableType&                   inh,
		const ErrorContextType&                  error_context_inh,
		base::HashMap<base::StrID, base::StrID>& vtable,
		vm::TypeMetadata&                        metadata,
		const TypeContext&                       ctx
	) {
		for (const auto& impl: inh.implementations) vtable.put(impl.name, impl.type);
		for (const auto& interface_name: inh.implements) {
			const auto& interface = getType<InterfaceType>(
				ctx,
				interface_name,
				error_context_inh,
				[&]() { return InvalidImplementsError(error_context_inh, interface_name); }
			);
			buildVTableRecursive(interface, error_context_inh, vtable, metadata, ctx);
		}
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			if (inh.extends) {
				const auto& super_class
					= getType<ClassType>(ctx, *inh.extends, error_context_inh, [&]() {
						  return InvalidExtendsError(inh, *inh.extends);
					  });
				buildVTableRecursive(super_class, error_context_inh, vtable, metadata, ctx);
			}
		}
	}

	/**
	 * @brief Builds a vector of fields (FieldVector) for an InheritableType. Collects all
	 * fields from superclasses.
	 */
	template<InheritableTypeConcept InheritableType>
	FieldVector buildFieldVector(
		const InheritableType& inh, vm::TypeMetadata& metadata, const TypeContext& ctx
	) {
		auto to_low_type = [&](const TypeOfData& tod) {
			return metadata.at(VISIT(tod, type, return type.name));
		};
		FieldVector fields{ { base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) } };

		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			std::function<void(const vm::code::ClassType&)> collect_class_fields_recursive
				= [&](const vm::code::ClassType& clazz) {
					  if (clazz.extends) {
						  const auto& super_class_code
							  = getType<ClassType>(ctx, *clazz.extends, clazz, [&]() {
									return InvalidExtendsError(clazz, *clazz.extends);
								});
						  collect_class_fields_recursive(super_class_code);
					  }

					  for (const Field& field_code: clazz.fields)
						  fields.emplace_back(field_code.name, metadata.at(field_code.type));
				  };
			collect_class_fields_recursive(inh);
		}
		return fields;
	}

	/**
	 * @brief Builds inheritance metadata for a given inheritable. Builds a field vector and a
	 * vtable.
	 */
	template<InheritableTypeConcept InheritableType>
	vm::InheritanceMetadata buildInheritanceMetadata(
		const InheritableType& inh, vm::TypeMetadata& metadata, const TypeContext& ctx
	) {
		vm::TypeCRef tp            = metadata.at(inh.name);
		auto         get_type_cref = [&](base::StrID name) -> vm::TypeCRef {
            return metadata.atMaybe(name).expect<UnknownSubtypeError>(inh, name);
		};
		auto implements = inh.implements | std::views::transform(get_type_cref)
		                | std::ranges::to<std::vector>();

		base::HashMap<base::StrID, vm::TypeCRef> virtual_methods;
		for (auto& method: inh.virtual_methods)
			virtual_methods.put(method.name, get_type_cref(method.type));
		// TODOP: Get rid of get_type_cref

		base::HashMap<base::StrID, base::StrID> vtable;
		buildVTableRecursive(inh, inh, vtable, metadata, ctx);

		vm::InheritanceMetadata::Kind kind;
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			kind = vm::InheritanceMetadata::Class{
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
}

Box<vm::TypeMetadata> vm::code::buildTypes(const TypeContext& ctx) {
	Box<vm::TypeMetadata> metadata = makeBox<vm::TypeMetadata>();

	auto types = ctx.getCurrentTypes();

	// Declare all types first.
	for (const auto& type: types) metadata->addType(vm::Type::declareType(typeName(type)));

	// Well-define every type.
	for (const auto& type: types) {
		variant_match(type) {
			variant_case(PrimitiveType, data) {
				metadata->at(data.name)->definePrimitive(data.size);
			}
			variant_case(PointerType, data) {
				metadata->at(data.name)->definePointer(metadata->at(data.inner));
			}
			variant_case(FixedSizeTableType, data) {
				metadata->at(data.name)->defineFixedSizeTable(
					metadata->at(data.inner), data.table_size
				);
			}
			variant_case(DynamicTableType, data) {
				metadata->at(data.name)->defineDynamicTable(metadata->at(data.inner));
			}
			variant_case(DataType, data) {
				FieldVector fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(field.name, metadata->at(field.type));
				metadata->at(data.name)->defineData(fields, {});
			}
			variant_case(VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(metadata->at(variant));
				metadata->at(data.name)->defineVariant(variants);
			}
			variant_case(FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters) parameters.emplace_back(metadata->at(param));
				metadata->at(data.name)->defineFunction(parameters, metadata->at(data.result));
			}
			variant_case(OpaqueType, opaque) {
				metadata->at(opaque.name)->defineOpaque(opaque.size);
			}
			variant_case(ClassType, clazz) {
				vm::TypeRef             tp     = metadata->at(clazz.name);
				FieldVector             fields = buildFieldVector(clazz, *metadata, ctx);
				vm::InheritanceMetadata inh_metadata
					= buildInheritanceMetadata(clazz, *metadata, ctx);
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_case(InterfaceType, interface) {
				vm::TypeRef             tp     = metadata->at(interface.name);
				FieldVector             fields = buildFieldVector(interface, *metadata, ctx);
				vm::InheritanceMetadata inh_metadata
					= buildInheritanceMetadata(interface, *metadata, ctx);
				tp->defineData(fields, std::move(inh_metadata));
			}
			variant_default {
				CORE_PANIC("Unhandled type during type building: ", typeToString(type));
			}
		}
	}
	metadata->finalize();
	return metadata;
}
