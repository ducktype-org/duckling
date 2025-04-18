#include "type_layout.hpp"

#include "queries.hpp"

#include <query_framework/context.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/variant.hpp>

using base::bytes2bits;

namespace tsl {
	namespace {
		/**
		 * @brief Get a vector of TypeLayouts for a vector of SymbolType.
		 * @param types The input SymbolType vector.
		 * @param ctx The query context.
		 * @return The output TypeLayout vector.
		 */
		std::vector<TypeLayout> getLayoutVector(
			const std::vector<tsh::SymbolType<>>& types, query::Context& ctx
		) {
			std::vector<TypeLayout> layouts;
			layouts.reserve(types.size());
			for (const auto& type: types) layouts.push_back(ctx.query<QuerySymbolTypeLayout>(type));
			return layouts;
		}

		/**
		 * @brief Gets the maximum size of a type layout in a vector.
		 * @param layouts Vector of type layouts to aggregate over.
		 * @return The maximum size of a type layout in the vector.
		 */
		Bits maxTypeLayoutSizeInVector(const std::vector<TypeLayout>& layouts) {
			Bits max{ 0 };
			for (const auto& type: layouts) max = std::max(max, type.getSize());
			return max;
		}

		/**
		 * @brief Gets the maximum size of a type in a vector.
		 * @param types Vector of types to aggregate over.
		 * @param ctx The Query Context necessary to deduce composite type sizes.
		 * @return The maximum size of a type in the vector.
		 */
		Bits maxTypeSizeInVector(const std::vector<tsh::SymbolType<>>& types, query::Context& ctx) {
			return maxTypeLayoutSizeInVector(getLayoutVector(types, ctx));
		}

		/**
		 * @brief Gets the offsets for the given type sizes, with alignment in mind.
		 * @param sizes The sizes of the types.
		 * @return The aligned offsets.
		 */
		std::vector<Bytes> alignOffsetsForSizeVector(const std::vector<Bits>& sizes) {
			// Preamble.
			std::vector<Bytes> offsets{};
			offsets.reserve(sizes.size());
			Bytes bytes_taken{ 0 };

			// For each component layout...
			for (const auto& size_in_bits: sizes) {
				// Get its size in bytes, rounded up.
				Bytes size_in_bytes{ (usize(size_in_bits) + 7) / 8 };

				// Find alignment factor.
				usize alignment_factor = 8;
				if (size_in_bytes <= Bytes(4)) alignment_factor = 4;
				if (size_in_bytes <= Bytes(2)) alignment_factor = 2;
				if (size_in_bytes <= Bytes(1)) alignment_factor = 1;

				// Round up offset to nearest multiple of alignment factor.
				bytes_taken = (bytes_taken + Bytes(alignment_factor - 1)) / alignment_factor
				            * alignment_factor;

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
		std::vector<Bytes> alignOffsetsForLayoutVector(const std::vector<TypeLayout>& layouts) {
			std::vector<Bits> sizes;
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
		std::vector<usize> offsetsToPermutation(const std::vector<Bytes>& offsets) {
			std::vector<std::pair<Bytes, usize>> offsets_with_idxs;
			std::vector<usize>                   result;
			offsets_with_idxs.reserve(offsets.size());
			result.reserve(offsets.size());

			for (usize component_idx = 0; component_idx < offsets.size(); component_idx++)
				offsets_with_idxs.emplace_back(offsets[component_idx], component_idx);
			std::sort(offsets_with_idxs.begin(), offsets_with_idxs.end());

			for (auto component_idx: offsets_with_idxs | std::views::values)
				result.push_back(component_idx);

			return result;
		}

		/**
		 * @brief Get a map from the index of appearance in a class layout to symbol ID of field.
		 * @param fields The interface elements representing the fields in a class.
		 * @param offsets The offsets of the @p fields, in the same order.
		 * @return A vector which has the field symbols in the order in which they appear in the
		 * layout.
		 */
		std::vector<compiler::helios::SymID> offsetsToSymIDs(
			const std::vector<tsh::InterfaceElement>& fields, const std::vector<Bytes>& offsets
		) {
			std::vector<usize>                   permutation = offsetsToPermutation(offsets);
			std::vector<compiler::helios::SymID> result;
			result.reserve(permutation.size());

			for (const usize field_idx: permutation)
				result.push_back(fields.at(field_idx).getSymbol());

			return result;
		}
	}

	struct VariantTypeLayoutConstructionHelper {
		tsh::VariantAbstractType variant_type;
		Bits                     max_component_size;
		std::vector<Bytes>       offsets;

		VariantTypeLayoutConstructionHelper(
			const tsh::VariantAbstractType variant_type, query::Context& ctx
		):
			  variant_type(variant_type),
			  max_component_size(maxTypeSizeInVector(variant_type.getUnderlyingTypes(), ctx)),
			  offsets(alignOffsetsForSizeVector({ Bits(8), max_component_size })) {}
	};

	VariantTypeLayout::VariantTypeLayout(
		const tsh::VariantAbstractType variant_type, query::Context& ctx
	):
		  VariantTypeLayout(VariantTypeLayoutConstructionHelper(variant_type, ctx)) {}

	VariantTypeLayout::VariantTypeLayout(VariantTypeLayoutConstructionHelper helper):
		  TypeLayoutABC(
			  bytes2bits(helper.offsets[1]) + helper.max_component_size, helper.variant_type
		  ),
		  tag_offset{ 0 },                   // 0 bytes
		  tag_size{ 8 },                     // 8 bits
		  data_offset{ helper.offsets[1] },  // up to 8 bytes
		  data_size{ helper.max_component_size },
		  index_to_type{ helper.variant_type.getUnderlyingTypes() } {
		for (u32 i = 0; i < index_to_type.size(); i++) {
			const auto& type = index_to_type[i];
			type_to_index.put(type, i);
		}
	}

	std::string VariantTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		std::stringstream ss{};

		// Display the tag and data sizes
		ss << getIndent(indent) << "variant (tag : " << std::to_string(tag_size)
		   << ", data : " << std::to_string(data_size) << ") {\n";

		// Display the components
		for (const auto component_type: index_to_type) {
			auto component_layout = ctx.query<QuerySymbolTypeLayout>(component_type);
			if (recursive)
				ss << component_layout.toStringDefinition(ctx, recursive, indent + 1) << "\n";
			else
				ss << getIndent(indent + 1) << component_layout.toStringIdentification() << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << std::to_string(getSize());

		return ss.str();
	}

	struct TupleTypeLayoutConstructionHelper {
		tsh::TupleAbstractType  tuple_type;
		std::vector<TypeLayout> component_layouts;
		std::vector<Bytes>      component_offsets;
		std::vector<usize>      offset_idx_to_component_idx;
		Bits                    total_size;

		TupleTypeLayoutConstructionHelper(
			const tsh::TupleAbstractType tuple_type, query::Context& ctx
		):
			  tuple_type(tuple_type),
			  component_layouts(getLayoutVector(tuple_type.getComponents(), ctx)),
			  component_offsets(alignOffsetsForLayoutVector(component_layouts)),
			  offset_idx_to_component_idx(offsetsToPermutation(component_offsets)),
			  total_size(
				  component_layouts.empty()
					  ? Bits(0)
					  : bytes2bits(component_offsets.back()) + component_layouts.back().getSize()
			  ) {}
	};

	TupleTypeLayout::TupleTypeLayout(const tsh::TupleAbstractType tuple_type, query::Context& ctx):
		  TupleTypeLayout(TupleTypeLayoutConstructionHelper(tuple_type, ctx)) {}

	TupleTypeLayout::TupleTypeLayout(TupleTypeLayoutConstructionHelper&& helper):
		  TypeLayoutABC(helper.total_size, helper.tuple_type),
		  component_offsets(std::move(helper).component_offsets),
		  offset_idx_to_component_idx(std::move(helper).offset_idx_to_component_idx) {}

	std::string TupleTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		const tsh::TupleAbstractType tuple_type = getSourceType();
		std::stringstream            ss{};

		// Display the tuple header and components
		ss << getIndent(indent) << "tuple {\n";
		for (const auto component_idx: offset_idx_to_component_idx) {
			const Bytes             component_offset = getComponentOffset(component_idx);
			const tsh::AbstractType component_type
				= tuple_type.getComponentAbstractTypes().at(component_idx);
			auto component_layout = ctx.query<QueryAbstractTypeLayout>(component_type);
			if (recursive)
				ss << component_layout.toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << component_layout.toStringIdentification();
			// Display the offset
			ss << " @ " << std::to_string(component_offset) << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << std::to_string(getSize());

		return ss.str();
	}

	struct ClassTypeLayoutConstructionHelper {
		tsh::ClassAbstractType               class_type;
		std::vector<tsh::InterfaceElement>   field_elements;
		std::vector<TypeLayout>              field_layouts;
		std::vector<Bytes>                   field_offsets;
		std::vector<compiler::helios::SymID> offset_idx_to_sym_id;
		Bits                                 total_size;

		static std::vector<tsh::InterfaceElement> getFieldsOfInterface(const tsh::TypeInterface&
		                                                                   interface) {
			const auto&                        elements = interface.getElements();
			std::vector<tsh::InterfaceElement> fields;
			fields.reserve(elements.size());

			for (const auto& val: elements | std::views::values) {
				for (const auto& element: val)
					if (element.isField()) fields.push_back(element);
			}

			return fields;
		}

		static std::vector<tsh::SymbolType<>> getElementTypes(
			const std::vector<tsh::InterfaceElement>& elements, query::Context& ctx
		) {
			std::vector<tsh::SymbolType<>> types;
			types.reserve(elements.size());
			for (const auto& element: elements) types.push_back(element.getType(ctx));
			return types;
		}

		ClassTypeLayoutConstructionHelper(
			const tsh::ClassAbstractType class_type, query::Context& ctx
		):
			  class_type(class_type),
			  field_elements(getFieldsOfInterface(class_type.getInterface(ctx))),
			  field_layouts(getLayoutVector(getElementTypes(field_elements, ctx), ctx)),
			  field_offsets(alignOffsetsForLayoutVector(field_layouts)),
			  offset_idx_to_sym_id(offsetsToSymIDs(field_elements, field_offsets)),
			  total_size(
				  field_layouts.empty()
					  ? Bits(0)
					  : bytes2bits(field_offsets.back()) + field_layouts.back().getSize()
			  ) {}
	};

	ClassTypeLayout::ClassTypeLayout(const tsh::ClassAbstractType class_type, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(class_type, ctx)) {}

	ClassTypeLayout::ClassTypeLayout(ClassTypeLayoutConstructionHelper&& helper):
		  TypeLayoutABC(helper.total_size, helper.class_type),
		  offset_idx_to_sym_id(std::move(helper).offset_idx_to_sym_id) {
		for (u32 i = 0; i < helper.field_elements.size(); i++)
			field_offsets.put(helper.field_elements[i].getSymbol(), helper.field_offsets[i]);
	}

	std::string ClassTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		const tsh::ClassAbstractType class_type = getSourceType();
		std::stringstream            ss{};

		// Display the class header and components
		ss << getIndent(indent) << class_type.toString() << " {\n";
		for (const auto field_sym_id: offset_idx_to_sym_id) {
			const Bytes             field_offset = getFieldOffset(field_sym_id);
			const tsh::SymbolType<> field_type   = class_type.getMemberType(field_sym_id, ctx);
			auto                    field_layout = ctx.query<QuerySymbolTypeLayout>(field_type);
			if (recursive)
				ss << field_layout.toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << field_layout.toStringIdentification();
			// Display the offset
			ss << " @ " << std::to_string(field_offset) << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << std::to_string(getSize());

		return ss.str();
	}

	PointerTypeLayout::PointerTypeLayout(
		const tsh::PointerAbstractType pointer_type, query::Context& ctx
	):
		  TypeLayoutABC(POINTER_SIZE, pointer_type),
		  pointee(makeBox<TypeLayout>(
			  ctx.query<QueryAbstractTypeLayout>(pointer_type.getUnderlyingType())
		  )) {}

	PointerTypeLayout::PointerTypeLayout(const tsh::SymbolType<> symbol_type, query::Context& ctx):
		  TypeLayoutABC(POINTER_SIZE, symbol_type.getType(), symbol_type.getRefKind()),
		  pointee(makeBox<TypeLayout>(ctx.query<QueryAbstractTypeLayout>(symbol_type.getType()))) {
		CORE_ASSERT(
			symbol_type.getRefKind() != tsh::ReferenceKind::Direct,
			"Construction of pointer layout from symbol type "
			"without reference indirection is forbidden."
		);
	}

	Bits TypeLayout::getSize() const { return VISIT(*this, l, return l.getSize()); }

	tsh::AbstractType TypeLayout::getSourceType() const {
		return VISIT(*this, l, return l.getSourceType());
	}

	std::string TypeLayout::toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const {
		return VISIT(*this, l, return l.toStringDefinition(ctx, recursive, indent));
	}

	std::string TypeLayout::toStringIdentification() const {
		return VISIT(*this, l, return l.toStringIdentification());
	}
}
