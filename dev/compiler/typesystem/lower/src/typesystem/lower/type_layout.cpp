#include "type_layout.hpp"
#include "queries.hpp"

#include <query_framework/query_impl.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <queue>
#include <ranges>

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

		/**
		 * @brief Get the permutation of where the component types land in a layout based on
		 * offsets.
		 * @param offsets The offsets of the component types.
		 * @return How the component types are permuted.
		 */
		std::vector<usize> offsetsToPermutation(const std::vector<usize>& offsets) {
			std::priority_queue<std::pair<usize, usize>> pq;
			std::vector<usize>                           result;
			result.resize(offsets.size());

			for (usize component_idx = 0; component_idx < offsets.size(); component_idx++)
				pq.push({ offsets[component_idx], component_idx });

			usize offset_idx = offsets.size() - 1;
			while (!pq.empty()) {
				result[offset_idx] = pq.top().second;
				pq.pop();
				offset_idx--;
			}

			return result;
		}

		std::vector<compiler::helios::SymID> offsetsToSymIDs(
			const std::vector<usize>& offsets, const std::vector<tsh::InterfaceElement> fields
		) {
			std::vector<usize>                   permutation = offsetsToPermutation(offsets);
			std::vector<compiler::helios::SymID> result;
			result.reserve(permutation.size());

			for (usize field_idx: permutation) result.push_back(fields.at(field_idx).getSymbol());

			return result;
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
		  data_size{ helper.max_component_size },
		  index_to_type{ helper.variant_info.getUnderlyingTypes() } {
		for (int i = 0; i < index_to_type.size(); i++) {
			const auto& type = index_to_type[i];
			type_to_index.put(type, i);
		}
	}

	std::string VariantTypeLayout::toStringDefinition(
		query::Context& ctx, bool recursive, u32 indent
	) const {
		std::stringstream ss{};

		// Display the tag and data sizes
		ss << getIndent(indent) << "variant (tag : " << std::to_string(tag_size)
		   << ", data : " << std::to_string(data_size) << ") {\n";

		// Display the components
		for (auto component_type: index_to_type) {
			auto component_layout = ctx.query<QueryTypeLayout>(component_type);
			if (recursive)
				ss << component_layout.toStringDefinition(ctx, recursive, indent + 1) << "\n";
			else
				ss << getIndent(indent + 1) << component_layout.toStringIdentification() << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << getSize();

		return ss.str();
	}

	struct TupleTypeLayoutConstructionHelper {
		tsh::TupleInfo          tuple_info;
		std::vector<TypeLayout> component_layouts;
		std::vector<usize>      component_offsets;
		std::vector<usize>      offset_idx_to_component_idx;
		usize                   total_size;

		TupleTypeLayoutConstructionHelper(tsh::TupleInfo tuple_info, query::Context& ctx):
			  tuple_info(tuple_info),
			  component_layouts(getLayoutVector(tuple_info.getComponentTypes(), ctx)),
			  component_offsets(alignOffsetsForLayoutVector(component_layouts)),
			  offset_idx_to_component_idx(offsetsToPermutation(component_offsets)),
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
		  component_offsets(std::move(helper).component_offsets),
		  offset_idx_to_component_idx(std::move(helper).offset_idx_to_component_idx) {}

	std::string
		TupleTypeLayout::toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const {
		tsh::TupleInfo    tuple_type = getSourceType();
		std::stringstream ss{};

		// Display the tuple header and components
		ss << getIndent(indent) << "tuple {\n";
		for (auto component_idx: offset_idx_to_component_idx) {
			usize         component_offset = getComponentOffset(component_idx);
			tsh::TypeInfo component_type   = tuple_type.getComponentTypes().at(component_idx);
			auto          component_layout = ctx.query<QueryTypeLayout>(component_type);
			if (recursive)
				ss << component_layout.toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << component_layout.toStringIdentification();
			// Display the offset
			ss << " @ " << component_offset << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << getSize();

		return ss.str();
	}

	struct ClassTypeLayoutConstructionHelper {
		tsh::ClassInfo                       class_info;
		std::vector<tsh::InterfaceElement>   field_elements;
		std::vector<TypeLayout>              field_layouts;
		std::vector<usize>                   field_offsets;
		std::vector<compiler::helios::SymID> offset_idx_to_sym_id;
		usize                                total_size;

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
			  offset_idx_to_sym_id(offsetsToSymIDs(field_offsets, field_elements)),
			  total_size(
				  field_layouts.empty()
					  ? 0
					  : field_offsets.back() * BYTE_SIZE + field_layouts.back().getSize()
			  ) {}
	};

	ClassTypeLayout::ClassTypeLayout(tsh::ClassInfo class_info, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(class_info, ctx)) {}

	ClassTypeLayout::ClassTypeLayout(ClassTypeLayoutConstructionHelper&& helper):
		  TypeLayoutABC(helper.total_size, helper.class_info),
		  offset_idx_to_sym_id(std::move(helper).offset_idx_to_sym_id) {
		for (int i = 0; i < helper.field_elements.size(); i++)
			field_offsets.put(helper.field_elements[i].getSymbol(), helper.field_offsets[i]);
	}

	std::string
		ClassTypeLayout::toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const {
		tsh::ClassInfo    class_type = getSourceType();
		std::stringstream ss{};

		// Display the class header and components
		ss << getIndent(indent) << class_type.toString() << " {\n";
		for (auto field_sym_id: offset_idx_to_sym_id) {
			usize         field_offset = getFieldOffset(field_sym_id);
			tsh::TypeInfo field_type   = class_type.getMemberType(field_sym_id, ctx);
			auto          field_layout = ctx.query<QueryTypeLayout>(field_type);
			if (recursive)
				ss << field_layout.toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << field_layout.toStringIdentification();
			// Display the offset
			ss << " @ " << field_offset << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << getSize();

		return ss.str();
	}

	PointerTypeLayout::PointerTypeLayout(tsh::PointerInfo pointer_info, query::Context& ctx):
		  TypeLayoutABC(POINTER_SIZE, pointer_info),
		  pointee(box<TypeLayout>(ctx.query<QueryTypeLayout>(pointer_info.getUnderlyingType()))) {}

	usize TypeLayout::getSize() const { return VISIT(*this, l, return l.getSize()); }

	tsh::TypeInfo TypeLayout::getSourceType() const {
		return VISIT(*this, l, return l.getSourceType());
	}

	std::string
		TypeLayout::toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const {
		return VISIT(*this, l, return l.toStringDefinition(ctx, recursive, indent));
	}

	std::string TypeLayout::toStringIdentification() const {
		return VISIT(*this, l, return l.toStringIdentification());
	}
}
