#include "type_builder.hpp"

#include <base/except/exceptions.hpp>

#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

using namespace vm::code;
using namespace vm::loader::compiler::fast;

namespace {
	void declareTypes(
		Ref<vm::fast::TypeCollection>                   type_collection,
		const std::vector<CRef<valid_type::ValidType>>& new_types
	) {
		for (CRef<valid_type::ValidType> type: new_types) {
			Bytes size(0);
			if (!type->isKind<vm::code::valid_type::finalized::DynamicTable>())
				size = type->getSize().assumePointerSize(Bytes(8));

			type_collection->insert(
				vm::fast::Type::declareType(
					type->getName(), vm::fast::TypeID(type->getID().asInt()), size
				),
				type->getName()
			);
		}
	}

	void defineTypes(
		Ref<vm::fast::TypeCollection>                         type_collection,
		const vm::code::valid_type::ValidTypeMap&             types_ctx,
		const std::vector<vm::code::valid_type::ValidTypeID>& new_types
	) {
		auto convert_type_ids_to_crefs = [&](const std::vector<valid_type::ValidTypeID>& type_ids) {
			return type_ids
			     | std::views::transform([&](valid_type::ValidTypeID id) -> vm::fast::TypeCRef {
					   return type_collection->at(vm::fast::TypeID(id.asInt()));
				   })
			     | std::ranges::to<std::vector>();
		};

		for (valid_type::ValidTypeID type_id: new_types) {
			const valid_type::ValidType& type = *types_ctx.at(type_id);
			variant_match(types_ctx.at(type_id)->getKind()) {
				variant_case(valid_type::finalized::Primitive, primitive) {
					type_collection->at(type.getName())->definePrimitive(primitive.size);
				}
				variant_case(valid_type::finalized::Pointer, pointer) {
					type_collection->at(type.getName())
						->definePointer(type_collection->at(types_ctx.at(pointer.inner)->getName()));
				}
				variant_case(valid_type::finalized::FixedSizeTable, fixed_size_table) {
					type_collection->at(type.getName())
						->defineFixedSizeTable(
							type_collection->at(types_ctx.at(fixed_size_table.inner)->getName()),
							fixed_size_table.element_count
						);
				}
				variant_case(valid_type::finalized::DynamicTable, dynamic_table) {
					type_collection->at(type.getName())
						->defineDynamicTable(
							type_collection->at(types_ctx.at(dynamic_table.inner)->getName())
						);
				}
				variant_case(valid_type::finalized::Structure, structure) {
					// @TODO: #2719 Implement structure type definition.
					throw base::NotYetImplemented(
						"Structure type definition is not implemented yet."
					);
				}
				variant_case(valid_type::finalized::Variant, variant) {
					type_collection->at(type.getName())
						->defineVariant(
							variant.type_tag_size,
							convert_type_ids_to_crefs(variant.alternatives_ordered)
						);
				}
				variant_case(valid_type::finalized::Function, function) {
					type_collection->at(type.getName())
						->defineFunction(
							convert_type_ids_to_crefs(function.parameters),
							convert_type_ids_to_crefs(function.result_types)
						);
				}
				variant_case(valid_type::finalized::Opaque, opaque) {
					type_collection->at(type.getName())->defineOpaque(opaque.size);
				}
				variant_default { CORE_PANIC("Unhandled type during type building"); }
			}
		}
	}
}

void vm::loader::compiler::fast::rebuildFastTypeCollection(
	Ref<vm::fast::TypeCollection> type_collection, const valid_type::ValidTypeMap& type_ctx
) {
	std::vector<CRef<valid_type::ValidType>> new_types_vec
		= type_ctx | std::views::drop(type_collection->size())
	    | std::views::transform([](const auto& type) { return CRef(&type); })
	    | std::ranges::to<std::vector>();

	// Declare new types.
	declareTypes(type_collection, new_types_vec);

	// Well define new types.
	defineTypes(
		type_collection, type_ctx, new_types_vec | std::views::transform([](const auto& type) {
									   return type->getID();
								   }) | std::ranges::to<std::vector>()
	);

	// Sanity assert
	for (const auto& type: type_ctx) {
		CORE_ASSERT(
			static_cast<usize>(type_collection->at(type.getName())->getID()) == type.getID().asInt(),
			"Type ID mismatch after rebuilding type collection"
		);
	}
}
