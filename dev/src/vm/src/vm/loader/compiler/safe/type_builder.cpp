#include "type_builder.hpp"

#include <base/except/exceptions.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/bytecode/validator/valid_type/type_map.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/type_metadata/inheritance_metadata.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <ranges>

namespace {
	/**
	 * @brief Builds inheritance metadata for a given inheritable. Builds a field vector and a
	 * vtable.
	 */
	base::Optional<vm::InheritanceMetadata> buildInheritanceMetadata(
		vm::TypeMetadata&                                 type_metadata,
		const vm::code::valid_type::ValidType&            type,
		const vm::code::valid_type::finalized::Structure& structure
	) {
		match_optional(structure.inheritance_metadata) {
			opt_some(prev_imd) {
				vm::TypeCRef tp = type_metadata.at(type.getName());

				auto get_typeid = [&](vm::code::valid_type::ValidTypeID type_id) -> vm::TypeID {
					return vm::TypeID(type_id.asInt());
				};
				auto get_type_cref
					= [&](vm::code::valid_type::ValidTypeID type_id) -> vm::TypeCRef {
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
						vm::code::valid_type::finalized::InheritanceMetadata::ClassKind, class_kind
					) {
						kind = vm::InheritanceMetadata::Class{
							.is_abstract = class_kind.is_abstract,
							.extends     = class_kind.extends.map(get_type_cref),
						};
					}
					variant_case(
						vm::code::valid_type::finalized::InheritanceMetadata::InterfaceKind,
						interface_kind
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
		Ref<vm::TypeMetadata>                                     type_metadata,
		const std::vector<CRef<vm::code::valid_type::ValidType>>& types
	) {
		for (const auto& type: types)
			type_metadata->addType(vm::Type::declareType(type->getName()));
	}

	/**
	 * @brief Define types from the list in the given type_metadata.
	 */
	void defineTypes(
		Ref<vm::TypeMetadata>                                 type_metadata,
		const vm::code::valid_type::ValidTypeMap&             types_ctx,
		const std::vector<vm::code::valid_type::ValidTypeID>& new_types
	) {
		using std::ranges::to;
		using std::views::transform;

		for (const auto& type_id: new_types) {
			const auto& type             = types_ctx.at(type_id);
			const auto& type_at_metadata = type_metadata->at(type->getName());
			variant_match(type->getKind()) {
				variant_case(vm::code::valid_type::finalized::Primitive, data) {
					type_at_metadata->definePrimitive(data.size);
				}
				variant_case(vm::code::valid_type::finalized::Pointer, data) {
					// Note the interesting cast from valid_type::ValidTypeID to vm::TypeID.
					// This is by convention, they have to be the same.
					type_at_metadata->definePointer(type_metadata->at(vm::TypeID(data.inner.asInt())
					));
				}
				variant_case(vm::code::valid_type::finalized::CPointer, data) {
					type_at_metadata->defineCPointer(
						data.inner.map([&](const auto& inner) -> vm::TypeCRef {
							return type_metadata->at(vm::TypeID(inner.asInt()));
						})
					);
				}
				variant_case(vm::code::valid_type::finalized::FixedSizeTable, data) {
					type_at_metadata->defineFixedSizeTable(
						type_metadata->at(vm::TypeID(data.inner.asInt())), data.element_count
					);
				}
				variant_case(vm::code::valid_type::finalized::DynamicTable, data) {
					type_at_metadata->defineDynamicTable(
						type_metadata->at(vm::TypeID(data.inner.asInt()))
					);
				}
				variant_case(vm::code::valid_type::finalized::Structure, data) {
					// The validator is the source of truth for layout: field offsets and the
					// total size are taken from it, resolved for this runtime's pointer width.
					auto resolve_size = [](const vm::code::valid_type::TypeSize& size) {
						return size.assumePointerSize(vm::Type::POINTER_SIZE);
					};
					std::vector<std::tuple<base::StrID, vm::TypeRef, vm::Offset>> fields
						= data.fields | transform([&](auto& field) {
							  return std::tuple{ field.name,
							                     type_metadata->at(vm::TypeID(field.type.asInt())),
							                     resolve_size(field.offset) };
						  })
					    | to<std::vector>();
					base::Optional<vm::InheritanceMetadata> inh_metadata
						= buildInheritanceMetadata(*type_metadata, *type, data);
					type_at_metadata->defineData(
						fields, resolve_size(type->getSize()), inh_metadata
					);
				}
				variant_case(vm::code::valid_type::finalized::Variant, data) {
					std::vector<vm::TypeRef> variants
						= data.alternatives_ordered | transform([&](auto& variant) -> vm::TypeRef {
							  return type_metadata->at(vm::TypeID(variant.asInt()));
						  })
					    | to<std::vector>();
					type_at_metadata->defineVariant(data.type_tag_size, variants);
				}
				variant_case(vm::code::valid_type::finalized::Function, data) {
					std::vector<vm::TypeCRef> parameters
						= data.parameters | transform([&](auto& param) -> vm::TypeCRef {
							  return type_metadata->at(vm::TypeID(param.asInt()));
						  })
					    | to<std::vector>();
					std::vector<vm::TypeCRef> result_types
						= data.result_types | transform([&](auto& res) -> vm::TypeCRef {
							  return type_metadata->at(vm::TypeID(res.asInt()));
						  })
					    | to<std::vector>();
					type_at_metadata->defineFunction(parameters, result_types);
				}
				variant_case(vm::code::valid_type::finalized::Opaque, opaque) {
					type_at_metadata->defineOpaque(opaque.size);
				}
				variant_default { CORE_PANIC("Unhandled type during type building"); }
			}
		}
	}
}

void vm::code::detail::rebuildTypeMetadata(
	Ref<vm::TypeMetadata> type_metadata, const valid_type::ValidTypeMap& types
) {
	std::vector<CRef<valid_type::ValidType>> types_vec
		= types | std::views::drop(type_metadata->size())
	    | std::views::transform([](const auto& type) { return CRef(&type); })
	    | std::ranges::to<std::vector>();

	// Reopen type metadata for addition.
	type_metadata->unfinalize();

	// Declare new types.
	declareTypes(type_metadata, types_vec);
	// Well define new types.
	defineTypes(type_metadata, types, types_vec | std::views::transform([](const auto& type) {
										  return type->getID();
									  }) | std::ranges::to<std::vector>());

	type_metadata->finalize();

	// Sanity assert
	for (const auto& type: types) {
		CORE_ASSERT(
			static_cast<usize>(type_metadata->at(type.getName())->getID()) == type.getID().asInt(),
			"Type ID mismatch after rebuilding type metadata"
		);
	}
}
