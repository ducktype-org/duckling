#include "type_layout.hpp"
#include "queries.hpp"

#include <query_framework/query_impl.hpp>
#include <typesystem/higher/type_interface.hpp>

namespace tsl {
	namespace {
		/**
		 * @brief Get a vector of TypeLayouts for a vector of TypeInfo.
		 * @param types The input TypeInfo vector.
		 * @param ctx The query context.
		 * @return The output TypeLayout vector.
		 */
		std::vector<TypeLayout>
			getLayoutVector(const std::vector<tsh::TypeInfo>& types, query::Context& ctx) {
			std::vector<tsl::TypeLayout> layouts;
			layouts.reserve(types.size());
			for (const auto& type: types) layouts.push_back(ctx.query<QueryTypeLayout>(type));
			return layouts;
		}

		/**
		 * @brief Gets the maximum size of a type layout in a vector.
		 * @param layouts Vector of type layouts to aggregate over.
		 * @return The maximum size of a type layout in the vector.
		 */
		usize maxTypeLayoutSizeInVector(const std::vector<TypeLayout>& layouts) {
			usize max = 0;
			for (const auto& type: layouts) max = std::max(max, type.getSize());
			return max;
		}

		/**
		 * @brief Gets the maximum size of a type in a vector.
		 * @param types Vector of types to aggregate over.
		 * @param ctx The Query Context necessary to deduce composite type sizes.
		 * @return The maximum size of a type in the vector.
		 */
		usize maxTypeSizeInVector(const std::vector<tsh::TypeInfo>& types, query::Context& ctx) {
			return maxTypeLayoutSizeInVector(getLayoutVector(types, ctx));
		}

		/**
		 * @brief Gets the offsets for the given type sizes, with alignment in mind.
		 * @param sizes The sizes of the types.
		 * @return The aligned offsets.
		 */
		std::vector<usize> alignOffsetsForSizeVector(const std::vector<usize>& sizes) {
			// Preamble.
			std::vector<usize> offsets{};
			offsets.reserve(sizes.size());
			usize bytes_taken = 0;

			// For each component layout...
			for (const auto& size_in_bits: sizes) {
				// Get its size in bytes, rounded up.
				usize size_in_bytes = (size_in_bits + 7) / 8;

				// Find alignment factor.
				usize alignment_factor = 8;
				if (size_in_bytes <= 4) alignment_factor = 4;
				if (size_in_bytes <= 2) alignment_factor = 2;
				if (size_in_bytes <= 1) alignment_factor = 1;

				// Round up offset to nearest multiple of alignment factor.
				bytes_taken
					= (bytes_taken + (alignment_factor - 1)) / alignment_factor * alignment_factor;

				// Save offset.
				offsets.push_back(bytes_taken);
				bytes_taken += size_in_bytes;
			}

			return offsets;
		}

		/**
		 * @brief Gets the offsets for the given type layouts, with alignment in mind.
		 * @param layouts The layouts of the types.
		 * @return The aligned offsets.
		 */
		std::vector<usize> alignOffsetsForLayoutVector(const std::vector<TypeLayout>& layouts) {
			std::vector<usize> sizes;
			sizes.reserve(layouts.size());
			for (const auto& layout: layouts) sizes.push_back(layout.getSize());
			return alignOffsetsForSizeVector(sizes);
		}
	}

	struct VariantTypeLayoutConstructionHelper {
		tsh::VariantInfo   variant_info;
		usize              max_component_size;
		std::vector<usize> offsets;

		VariantTypeLayoutConstructionHelper(tsh::VariantInfo variant_info, query::Context& ctx):
			  variant_info(variant_info),
			  max_component_size(maxTypeSizeInVector(variant_info.getUnderlyingTypes(), ctx)),
			  offsets(alignOffsetsForSizeVector({ 8, max_component_size })) {}
	};

	VariantTypeLayout::VariantTypeLayout(tsh::VariantInfo variant_info, query::Context& ctx):
		  VariantTypeLayout(VariantTypeLayoutConstructionHelper(variant_info, ctx)) {}

	VariantTypeLayout::VariantTypeLayout(VariantTypeLayoutConstructionHelper helper):
		  TypeLayoutABC(
			  helper.offsets[1] * BYTE_SIZE + helper.max_component_size, helper.variant_info
		  ),
		  tag_offset{ 0 },                   // 0 bytes
		  tag_size{ 8 },                     // 8 bits
		  data_offset{ helper.offsets[1] },  // up to 8 bytes
		  index_to_type{ helper.variant_info.getUnderlyingTypes() } {
		for (int i = 0; i < index_to_type.size(); i++) {
			const auto& type = index_to_type[i];
			type_to_index.put(type, i);
		}
	}

	struct TupleTypeLayoutConstructionHelper {
		tsh::TupleInfo          tuple_info;
		std::vector<TypeLayout> component_layouts;
		std::vector<usize>      component_offsets;
		usize                   total_size;

		TupleTypeLayoutConstructionHelper(tsh::TupleInfo tuple_info, query::Context& ctx):
			  tuple_info(tuple_info),
			  component_layouts(getLayoutVector(tuple_info.getComponentTypes(), ctx)),
			  component_offsets(alignOffsetsForLayoutVector(component_layouts)),
			  total_size(
				  component_layouts.empty()
					  ? 0
					  : component_offsets.back() * BYTE_SIZE + component_layouts.back().getSize()
			  ) {}
	};

	TupleTypeLayout::TupleTypeLayout(tsh::TupleInfo tuple_info, query::Context& ctx):
		  TupleTypeLayout(TupleTypeLayoutConstructionHelper(tuple_info, ctx)) {}

	TupleTypeLayout::TupleTypeLayout(TupleTypeLayoutConstructionHelper&& helper):
		  TypeLayoutABC(helper.total_size, helper.tuple_info),
		  component_offsets(std::move(helper).component_offsets) {}

	struct ClassTypeLayoutConstructionHelper {
		tsh::ClassInfo                     class_info;
		std::vector<tsh::InterfaceElement> field_elements;
		std::vector<TypeLayout>            field_layouts;
		std::vector<usize>                 field_offsets;
		usize                              total_size;

		static std::vector<tsh::InterfaceElement>
			getFieldsOfInterface(const tsh::TypeInterface& interface) {
			const auto&                        elements = interface.getElements();
			std::vector<tsh::InterfaceElement> fields;
			fields.reserve(elements.size());

			for (const auto& elements_with_name: elements) {
				for (const auto& element: elements_with_name.second)
					if (element.isField()) fields.push_back(element);
			}

			return fields;
		}

		static std::vector<tsh::TypeInfo> getElementTypes(
			const std::vector<tsh::InterfaceElement>& elements, query::Context& ctx
		) {
			std::vector<tsh::TypeInfo> types;
			types.reserve(elements.size());
			for (const auto& element: elements) types.push_back(element.getType(ctx));
			return types;
		}

		ClassTypeLayoutConstructionHelper(tsh::ClassInfo class_info, query::Context& ctx):
			  class_info(class_info),
			  field_elements(getFieldsOfInterface(class_info.getInterface(ctx))),
			  field_layouts(getLayoutVector(getElementTypes(field_elements, ctx), ctx)),
			  field_offsets(alignOffsetsForLayoutVector(field_layouts)),
			  total_size(
				  field_layouts.empty()
					  ? 0
					  : field_offsets.back() * BYTE_SIZE + field_layouts.back().getSize()
			  ) {}
	};

	ClassTypeLayout::ClassTypeLayout(tsh::ClassInfo class_info, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(class_info, ctx)) {}

	ClassTypeLayout::ClassTypeLayout(const ClassTypeLayoutConstructionHelper& helper):
		  TypeLayoutABC(helper.total_size, helper.class_info) {
		for (int i = 0; i < helper.field_elements.size(); i++)
			field_offsets.put(helper.field_elements[i].getSymbol(), helper.field_offsets[i]);
	}

	PointerTypeLayout::PointerTypeLayout(tsh::PointerInfo pointer_info, query::Context& ctx):
		  TypeLayoutABC(POINTER_SIZE, pointer_info),
		  pointee(box<TypeLayout>(ctx.query<QueryTypeLayout>(pointer_info.getUnderlyingType()))) {}

	usize TypeLayout::getSize() const {
		usize result{};
		VISIT(*this, l, result = l.getSize());
		return result;
	}

	tsh::TypeInfo TypeLayout::getSourceType() const {
		base::Optional<tsh::TypeInfo> result;
		VISIT(*this, l, result = l.getSourceType());
		return result.value();
	}
}
