#include "type_layout.hpp"

#include "queries.hpp"

#include <helios/mangler/mangler.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>

#include <sstream>

using base::bytes2bits;

namespace compiler::tsl {
	namespace {
		/**
		 * @brief Get a vector of TypeLayouts for a vector of SymbolType.
		 * @param types The input SymbolType vector.
		 * @param ctx The query context.
		 * @return The output TypeLayout vector.
		 */
		std::vector<CRef<TypeLayout>> getLayoutVector(
			const std::vector<tsh::SymbolType<>>& types, query::Context& ctx
		) {
			std::vector<CRef<TypeLayout>> layouts;
			layouts.reserve(types.size());
			for (const auto& type: types) layouts.push_back(ctx.query<QuerySymbolTypeLayout>(type));
			return layouts;
		}

		/**
		 * @brief Gets the maximum size of a type layout in a vector.
		 * @param layouts Vector of type layouts to aggregate over.
		 * @return The maximum size of a type layout in the vector.
		 */
		Bits maxTypeLayoutSizeInVector(const std::vector<CRef<TypeLayout>>& layouts) {
			Bits max{ 0 };
			for (const auto& type: layouts) max = std::max(max, type->getSize());
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
		std::vector<Bytes> alignOffsetsForLayoutVector(const std::vector<CRef<TypeLayout>>& layouts
		) {
			std::vector<Bits> sizes;
			sizes.reserve(layouts.size());
			for (const auto& layout: layouts) sizes.push_back(layout->getSize());
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
		std::vector<compiler::helios::SymID> getLayoutIndicesToSymIDs(
			const std::vector<tsh::InterfaceElement>& fields, const std::vector<Bytes>& offsets
		) {
			CORE_ASSERT(fields.size() == offsets.size(), "Input vectors must have the same size.");
			std::vector<usize>                   permutation = offsetsToPermutation(offsets);
			std::vector<compiler::helios::SymID> result;
			result.reserve(permutation.size());

			for (const usize field_idx: permutation)
				result.push_back(fields.at(field_idx).getSymbol());

			return result;
		}
	}

	TypeLayoutABC::TypeLayoutABC(
		const Bits size, const tsh::SymbolType<> source_type, query::Context& ctx
	):
		  size(size),
		  source_type(source_type),
		  mangled_name(ctx.query<helios::mangler::QueryMangledType>(source_type)->valueOrThrow()) {}

	DynamicArrayTypeLayout::DynamicArrayTypeLayout(
		const tsh::DynamicArrayAbstractType dynamic_array_type, query::Context& ctx
	):
		  TypeLayoutABC(
			  POINTER_SIZE + bytes2bits(METADATA_SIZE) * 3,
			  tsh::SymbolType<>::withDefaults(dynamic_array_type),
			  ctx
		  ),
		  element_layout(ctx.query<QuerySymbolTypeLayout>(dynamic_array_type.getElementType())) {}

	std::string DynamicArrayTypeLayout::toStringDefinition(
		query::Context& ctx, bool recursive, const u32 indent
	) const {
		std::stringstream ss{};
		ss << getIndent(indent) << "dynamic_array {\n";

		// Display the element layout
		if (recursive)
			ss << element_layout->toStringDefinition(ctx, recursive, indent + 1) << "\n";
		else
			ss << getIndent(indent + 1) << element_layout->toStringIdentification() << "\n";

		// Display the total size
		ss << getIndent(indent) << "} : " << base::toString(getSize());
		return ss.str();
	}

	StaticArrayTypeLayout::StaticArrayTypeLayout(
		const tsh::StaticArrayAbstractType static_array_type, query::Context& ctx
	):
		  TypeLayoutABC(
			  ctx.query<QuerySymbolTypeLayout>(static_array_type.getElementType())->getSize()
				  * static_array_type.getSize(),
			  tsh::SymbolType<>::withDefaults(static_array_type),
			  ctx
		  ),
		  element_layout(ctx.query<QuerySymbolTypeLayout>(static_array_type.getElementType())),
		  element_count(static_array_type.getSize()) {}

	std::string StaticArrayTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		std::stringstream ss{};
		ss << getIndent(indent) << "static_array [" << getElementCount() << "] {\n";

		if (recursive)
			ss << element_layout->toStringDefinition(ctx, recursive, indent + 1) << "\n";
		else
			ss << getIndent(indent + 1) << element_layout->toStringIdentification() << "\n";

		ss << getIndent(indent) << "} : " << base::toString(getSize());
		return ss.str();
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
		  VariantTypeLayout(VariantTypeLayoutConstructionHelper(variant_type, ctx), ctx) {}

	VariantTypeLayout::VariantTypeLayout(
		const VariantTypeLayoutConstructionHelper& helper, query::Context& ctx
	):
		  TypeLayoutABC(
			  bytes2bits(helper.offsets[1]) + helper.max_component_size,
			  tsh::SymbolType<>::withDefaults(helper.variant_type),
			  ctx
		  ),
		  tag_offset{ 0 },                   // 0 bytes
		  tag_size{ 8 },                     // 8 bits
		  data_offset{ helper.offsets[1] },  // up to 8 bytes
		  data_size{ helper.max_component_size } {
		u32 i = 0;
		for (auto type: helper.variant_type.getUnderlyingTypes()) {
			type_to_index.put(type, i);
			index_to_layout.push_back(ctx.query<QuerySymbolTypeLayout>(type));
			i++;
		}
	}

	std::string VariantTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		std::stringstream ss{};

		// Display the tag and data sizes
		ss << getIndent(indent) << "variant (tag : " << base::toString(tag_size)
		   << ", data : " << base::toString(data_size) << ") {\n";

		// Display the components
		for (const auto component_layout: index_to_layout)
			if (recursive)
				ss << component_layout->toStringDefinition(ctx, recursive, indent + 1) << "\n";
			else
				ss << getIndent(indent + 1) << component_layout->toStringIdentification() << "\n";

		// Display the total size
		ss << getIndent(indent) << "} : " << base::toString(getSize());

		return ss.str();
	}

	struct TupleTypeLayoutConstructionHelper final {
		tsh::TupleAbstractType tuple_type;
		/**
		 * The layouts of the components in the abstract tuple type.
		 */
		std::vector<CRef<TypeLayout>> component_layouts;
		/**
		 * The offsets of the components in the abstract tuple type.
		 * Note, the offsets are not necessarily increasing.
		 */
		std::vector<Bytes> component_offsets;
		/**
		 * Mapping from the order of appearance of sub-objects in the layout
		 * to the index of the component in the abstract tuple type.
		 * @note This is effectively a permutation represented by an integer vector.
		 */
		std::vector<usize> layout_idx_to_component_idx;
		/**
		 * The layouts of the sub-objects, in the order of appearance in the tuple layout.
		 */
		std::vector<CRef<TypeLayout>> layout_idx_to_component_layout;
		Bits                          total_size;

		TupleTypeLayoutConstructionHelper(
			const tsh::TupleAbstractType tuple_type, query::Context& ctx
		):
			  tuple_type(tuple_type),
			  component_layouts(getLayoutVector(tuple_type.getComponents(), ctx)),
			  component_offsets(alignOffsetsForLayoutVector(component_layouts)),
			  layout_idx_to_component_idx(offsetsToPermutation(component_offsets)),
			  total_size(
				  component_layouts.empty()
					  ? Bits(0)
					  : bytes2bits(component_offsets.back()) + component_layouts.back()->getSize()
			  ) {
			layout_idx_to_component_layout.reserve(component_layouts.size());
			for (const auto component_idx: layout_idx_to_component_idx)
				layout_idx_to_component_layout.push_back(component_layouts.at(component_idx));
		}
	};

	TupleTypeLayout::TupleTypeLayout(const tsh::TupleAbstractType tuple_type, query::Context& ctx):
		  TupleTypeLayout(TupleTypeLayoutConstructionHelper(tuple_type, ctx), ctx) {}

	TupleTypeLayout::TupleTypeLayout(TupleTypeLayoutConstructionHelper&& helper, query::Context& ctx):
		  TypeLayoutABC(helper.total_size, tsh::SymbolType<>::withDefaults(helper.tuple_type), ctx),
		  num_components(helper.component_layouts.size()),
		  component_offsets(std::move(helper).component_offsets),
		  layout_idx_to_component_idx(std::move(helper).layout_idx_to_component_idx),
		  layout_idx_to_layout(std::move(helper).layout_idx_to_component_layout) {
		component_idx_to_layout_idx.resize(num_components);
		for (u32 i = 0; i < num_components; i++)
			component_idx_to_layout_idx.at(layout_idx_to_component_idx.at(i)) = i;
	}

	std::string TupleTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		const tsh::TupleAbstractType tuple_type = getSourceType().getType();
		std::stringstream            ss{};

		// Display the tuple header and components
		ss << getIndent(indent) << "tuple {\n";
		for (const auto component_idx: layout_idx_to_component_idx) {
			const Bytes             component_offset = getOffsetOfComponentIndex(component_idx);
			const tsh::SymbolType<> component_type   = tuple_type.getComponents().at(component_idx);
			auto component_layout = ctx.query<QuerySymbolTypeLayout>(component_type);
			if (recursive)
				ss << component_layout->toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << component_layout->toStringIdentification();
			// Display the offset
			ss << " @ " << base::toString(component_offset) << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << base::toString(getSize());

		return ss.str();
	}

	struct ClassTypeLayoutConstructionHelper final {
		tsh::ClassAbstractType class_type;
		/**
		 * The fields of the class, in declaration order.
		 */
		std::vector<tsh::InterfaceElement> field_elements;
		/**
		 * The layouts of the fields, in declaration order.
		 */
		std::vector<CRef<TypeLayout>> field_layouts;
		/**
		 * The offsets of the fields, in declaration order.
		 * Note, the offsets are not necessarily increasing.
		 */
		std::vector<Bytes> field_offsets;
		/**
		 * Mapping from the order of appearance of sub-objects
		 * in the layout to the declaration index of the field.
		 */
		std::vector<usize> layout_idx_to_field_idx;
		/**
		 * Mapping from the order of appearance of sub-objects
		 * in the layout to the symbol ID of the field.
		 */
		std::vector<compiler::helios::SymID> layout_idx_to_sym_id;
		Bits                                 total_size;

		static std::vector<tsh::InterfaceElement> getFieldsOfInterface(CRef<tsh::TypeInterface>
		                                                                   interface) {
			const auto&                        elements = interface->getElements();
			std::vector<tsh::InterfaceElement> fields;
			fields.reserve(elements.size());

			for (const auto& element: elements)
				if (element.isField()) fields.push_back(element);

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
			  layout_idx_to_field_idx(offsetsToPermutation(field_offsets)),
			  layout_idx_to_sym_id(getLayoutIndicesToSymIDs(field_elements, field_offsets)),
			  total_size(
				  field_layouts.empty()
					  ? Bits(0)
					  : bytes2bits(field_offsets.back()) + field_layouts.back()->getSize()
			  ) {}
	};

	ClassTypeLayout::ClassTypeLayout(const tsh::ClassAbstractType class_type, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(class_type, ctx), ctx) {}

	ClassTypeLayout::ClassTypeLayout(ClassTypeLayoutConstructionHelper&& helper, query::Context& ctx):
		  TypeLayoutABC(helper.total_size, tsh::SymbolType<>::withDefaults(helper.class_type), ctx),
		  num_fields(helper.field_layouts.size()),
		  layout_idx_to_sym_id(std::move(helper).layout_idx_to_sym_id) {
		for (u32 i = 0; i < num_fields; i++) {
			// Fill out the map based on the subsequent field symbols and their offsets.
			sym_id_to_offset.put(
				helper.field_elements.at(i).getSymbol(), helper.field_offsets.at(i)
			);
		}
		for (u32 i = 0; i < num_fields; i++) {
			// Fill out the map as the inverse of layout_idx_to_sym_id.
			sym_id_to_layout_idx.put(layout_idx_to_sym_id.at(i), i);
		}
		layout_idx_to_layout.reserve(num_fields);
		for (u32 i = 0; i < num_fields; i++) {
			// Fill out the map based on the field layouts of subsequent layout components.
			layout_idx_to_layout.push_back(
				helper.field_layouts.at(helper.layout_idx_to_field_idx.at(i))
			);
		}
	}

	std::string ClassTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		const tsh::SymbolType<tsh::ClassAbstractType> class_type = getSourceType();
		std::stringstream                             ss{};

		// Display the class header and components
		ss << getIndent(indent) << class_type.toString() << " {\n";
		for (const auto field_sym_id: layout_idx_to_sym_id) {
			const Bytes             field_offset = getOffsetOfFieldSymbol(field_sym_id);
			const tsh::SymbolType<> field_type
				= class_type.getType().getMemberType(field_sym_id, ctx);
			const auto field_layout = ctx.query<QuerySymbolTypeLayout>(field_type);
			if (recursive)
				ss << field_layout->toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << field_layout->toStringIdentification();
			// Display the offset
			ss << " @ " << base::toString(field_offset) << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << base::toString(getSize());

		return ss.str();
	}

	PointerTypeLayout::PointerTypeLayout(
		const tsh::PointerAbstractType pointer_type, query::Context& ctx
	):
		  TypeLayoutABC(POINTER_SIZE, tsh::SymbolType<>::withDefaults(pointer_type), ctx),
		  pointee(ctx.query<QueryAbstractTypeLayout>(pointer_type.getUnderlyingType())) {}

	PointerTypeLayout::PointerTypeLayout(const tsh::SymbolType<> symbol_type, query::Context& ctx):
		  TypeLayoutABC(POINTER_SIZE, symbol_type, ctx),
		  pointee(ctx.query<QueryAbstractTypeLayout>(symbol_type.getType())) {
		CORE_ASSERT(
			symbol_type.getRefKind() != tsh::ReferenceKind::Direct,
			"Construction of pointer layout from symbol type "
			"without reference indirection is forbidden."
		);
	}

	Bits TypeLayout::getSize() const { return VISIT(variant, l, return l.getSize()); }

	tsh::SymbolType<> TypeLayout::getSourceType() const {
		return VISIT(variant, l, return l.getSourceType());
	}

	std::string TypeLayout::toStringDefinition(query::Context& ctx, bool recursive, u32 indent)
		const {
		return VISIT(variant, l, return l.toStringDefinition(ctx, recursive, indent));
	}

	std::string TypeLayout::toStringIdentification() const {
		return VISIT(variant, l, return l.toStringIdentification());
	}

	base::StrID TypeLayout::getMangledName() const {
		return VISIT(variant, l, return l.getMangledName());
	}
}
