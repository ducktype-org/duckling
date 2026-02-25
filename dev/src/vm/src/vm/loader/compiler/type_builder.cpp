#include "type_builder.hpp"

#include <base/except/exceptions.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/type.hpp>
#include <vm/bytecode/validator/type_context.hpp>
#include <vm/core/process/type_metadata/inheritance_metadata.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <ranges>

namespace {
	/**
	 * @brief Builds inheritance metadata for a given inheritable. Builds a field vector and a
	 * vtable.
	 */
	base::Optional<vm::InheritanceMetadata> buildInheritanceMetadata(
		vm::TypeMetadata&                          type_metadata,
		const vm::code::type::Type&                type,
		const vm::code::type::concrete::Structure& structure
	) {
		match_optional(structure.inheritance_metadata) {
			opt_some(prev_imd) {
				vm::TypeCRef tp = type_metadata.at(type.getName());

				auto get_typeid = [&](vm::code::type::TypeID type_id) -> vm::TypeID {
					return vm::TypeID(type_id);
				};
				auto get_type_cref = [&](vm::code::type::TypeID type_id) -> vm::TypeCRef {
					return type_metadata.at(get_typeid(type_id));
				};
				auto implements = prev_imd.implements | std::views::transform(get_type_cref)
				                | std::ranges::to<std::unordered_set>();

				base::HashMap<base::StrID, vm::TypeCRef> virtual_methods;
				for (auto& method: prev_imd.available_methods)
					virtual_methods.put(method.first, get_type_cref(method.second));

				base::HashMap<base::StrID, base::StrID> vtable;
				for (auto& impl: prev_imd.vtable) vtable.put(impl.first, impl.second);

				vm::InheritanceMetadata::Kind kind;
				variant_match(prev_imd.kind) {
					variant_case(
						vm::code::type::concrete::InheritanceMetadata::ClassKind, class_kind
					) {
						kind = vm::InheritanceMetadata::Class{
							.is_abstract = class_kind.is_abstract,
							.extends     = class_kind.extends.map(get_type_cref),
						};
					}
					variant_case(
						vm::code::type::concrete::InheritanceMetadata::InterfaceKind, interface_kind
					) {
						kind = vm::InheritanceMetadata::Interface{};
					}
					variant_default { CORE_PANIC("Unhandled inheritance metadata kind"); }
				}

				return vm::InheritanceMetadata{
					tp, kind, std::move(implements), std::move(virtual_methods), std::move(vtable),
				};
			}
			opt_none { return {}; }
		}
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Declare types from a list in the given type_metadata.
	 */
	void declareTypes(
		Ref<vm::TypeMetadata> type_metadata, const std::vector<CRef<vm::code::type::Type>>& types
	) {
		for (const auto& type: types)
			type_metadata->addType(vm::Type::declareType(type->getName()));
	}

	/**
	 * @brief Define types from the list in the given type_metadata.
	 */
	void defineTypes(
		Ref<vm::TypeMetadata>                      type_metadata,
		const vm::code::TypeMap&                   types_ctx,
		const std::vector<vm::code::type::TypeID>& new_types
	) {
		for (const auto& type_id: new_types) {
			const auto& type             = types_ctx.at(type_id);
			const auto& type_at_metadata = type_metadata->at(type->getName());
			variant_match(type->getKind()) {
				variant_case(vm::code::type::concrete::Primitive, data) {
					type_at_metadata->definePrimitive(static_cast<usize>(data.size));
				}
				variant_case(vm::code::type::concrete::Pointer, data) {
					// Note the interesting cast from type::TypeID to vm::TypeID.
					// This is by convention, they have to be the same.
					type_at_metadata->definePointer(type_metadata->at(vm::TypeID(data.inner)));
				}
				variant_case(vm::code::type::concrete::FixedSizeTable, data) {
					type_at_metadata->defineFixedSizeTable(
						type_metadata->at(vm::TypeID(data.inner)), data.element_count
					);
				}
				variant_case(vm::code::type::concrete::DynamicTable, data) {
					type_at_metadata->defineDynamicTable(type_metadata->at(vm::TypeID(data.inner)));
				}
				variant_case(vm::code::type::concrete::Structure, data) {
					std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
					fields.reserve(data.fields.size());
					for (auto& field: data.fields)
						fields.emplace_back(field.name, type_metadata->at(vm::TypeID(field.type)));
					base::Optional<vm::InheritanceMetadata> inh_metadata
						= buildInheritanceMetadata(*type_metadata, *type, data);
					type_at_metadata->defineData(fields, inh_metadata);
				}
				variant_case(vm::code::type::concrete::Variant, data) {
					std::vector<vm::TypeRef> variants;
					variants.reserve(data.alternatives.size());
					for (auto& variant: data.alternatives)
						variants.emplace_back(type_metadata->at(vm::TypeID(variant)));
					type_at_metadata->defineVariant(variants);
				}
				variant_case(vm::code::type::concrete::Function, data) {
					std::vector<vm::TypeCRef> parameters;
					parameters.reserve(data.parameters.size());
					for (auto& param: data.parameters)
						parameters.emplace_back(type_metadata->at(vm::TypeID(param)));
					type_at_metadata->defineFunction(
						parameters, type_metadata->at(vm::TypeID(data.result))
					);
				}
				variant_case(vm::code::type::concrete::Opaque, opaque) {
					type_at_metadata->defineOpaque(static_cast<usize>(opaque.size));
				}
				variant_default { CORE_PANIC("Unhandled type during type building"); }
			}
		}
	}
}

Box<vm::TypeMetadata> vm::code::detail::buildTypeMetadata(const TypeContext& types) {
	Box<vm::TypeMetadata> type_metadata = makeBox<vm::TypeMetadata>();

	std::vector<CRef<type::Type>> types_vec
		= types.getCurrentTypes()
	    | std::views::transform([](const auto& type) { return CRef(&type); })
	    | std::ranges::to<std::vector>();

	// Declare all types first.
	declareTypes(type_metadata.refMut(), types_vec);

	// Well-define every type.
	defineTypes(
		type_metadata.refMut(),
		types.getCurrentTypes(),
		types_vec | std::views::transform([](const auto& type) { return type->getID(); })
			| std::ranges::to<std::vector>()
	);

	type_metadata->finalize();
	return type_metadata;
}

void vm::code::detail::rebuildTypeMetadata(
	Ref<vm::TypeMetadata> type_metadata, const TypeContext& types
) {
	std::vector<CRef<type::Type>> types_vec
		= types.getCurrentTypes() | std::views::drop(type_metadata->size())
	    | std::views::transform([](const auto& type) { return CRef(&type); })
	    | std::ranges::to<std::vector>();

	// Reopen type metadata for addition.
	type_metadata->unfinalize();

	// Declare new types.
	declareTypes(type_metadata, types_vec);
	// Well define new types.
	defineTypes(
		type_metadata,
		types.getCurrentTypes(),
		types_vec | std::views::transform([](const auto& type) { return type->getID(); })
			| std::ranges::to<std::vector>()
	);

	type_metadata->finalize();
}
