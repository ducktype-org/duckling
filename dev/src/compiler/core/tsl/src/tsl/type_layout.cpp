#include "type_layout.hpp"

#include "c_abi_converter.hpp"
#include "c_abi_target.hpp"
#include "queries.hpp"

#include <abi/layout/compute_c_layout.hpp>
#include <abi/type_system/type.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/errors/extern_c_class_empty.hpp>
#include <helios/errors/field_not_c_compatible.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

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
			for (const auto& type: types)
				layouts.emplace_back(&ctx.query<QuerySymbolTypeLayout>(type)->valueOrThrow());
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
		std::vector<base::Optional<Bytes>> alignOffsetsForLayoutVector(
			const std::vector<CRef<TypeLayout>>& layouts
		) {
			std::vector<base::Optional<Bytes>> offsets{};
			offsets.reserve(layouts.size());
			Bytes bytes_taken{ 0 };

			for (const auto& layout: layouts) {
				// Skip empty layout
				if (layout->is<EmptyTypeLayout>()) {
					offsets.emplace_back();
					continue;
				}

				const Bytes size_in_bytes = base::bits2bytesRoundUp(layout->getSize());
				const usize alignment     = usize(layout->getAlignment());

				// Round up offset to nearest multiple of alignment.
				bytes_taken = (bytes_taken + Bytes(alignment - 1)) / alignment * alignment;

				// Save offset.
				offsets.emplace_back(bytes_taken);
				bytes_taken += size_in_bytes;
			}

			return offsets;
		}

		/**
		 * @brief Gets the maximum alignment of a type layout in a vector.
		 * @param layouts Vector of type layouts to aggregate over.
		 * @return The maximum alignment of a type layout in the vector.
		 */
		Bytes maxTypeLayoutAlignmentInVector(const std::vector<CRef<TypeLayout>>& layouts) {
			Bytes max{ 1 };
			for (const auto& layout: layouts) max = std::max(max, layout->getAlignment());
			return max;
		}

		/**
		 * @brief Get the permutation of where the component types land in a layout based on their
		 * offsets. Indices corresponding to empty components are omitted.
		 * For example, for a tuple type (i32, (), f64), the offsets might be [0, -, 8], and the
		 * resulting permutation would be [0, 2].
		 * @param offsets The offsets of the component types.
		 * @return How the component types are permuted.
		 */
		std::vector<usize> offsetsToPermutation(const std::vector<base::Optional<Bytes>>& offsets) {
			std::vector<std::pair<Bytes, usize>> offsets_with_idxs;
			std::vector<usize>                   result;
			offsets_with_idxs.reserve(offsets.size());
			result.reserve(offsets.size());

			for (usize component_idx = 0; component_idx < offsets.size(); component_idx++)
				if (auto offset = offsets[component_idx]; offset.has_value())
					offsets_with_idxs.emplace_back(offset.value(), component_idx);
			std::sort(offsets_with_idxs.begin(), offsets_with_idxs.end());

			for (auto component_idx: offsets_with_idxs | std::views::values)
				result.push_back(component_idx);

			return result;
		}

		Bits offsetsToTotalSize(
			const std::vector<base::Optional<Bytes>>& offsets,
			const std::vector<CRef<TypeLayout>>&      layouts
		) {
			Bytes total_size{ 0 };
			Bytes max_alignment{ 1 };
			for (usize component_idx = 0; component_idx < offsets.size(); component_idx++)
				if (auto offset = offsets[component_idx]; offset.has_value()) {
					const auto layout_size = layouts[component_idx]->getSize();
					const auto layout_end  = offset.value() + base::bits2bytesRoundUp(layout_size);
					total_size             = std::max(total_size, layout_end);
					max_alignment = std::max(max_alignment, layouts[component_idx]->getAlignment());
				}
			return bytes2bits(base::bytesRoundTo(total_size, max_alignment));
		}

		/**
		 * @brief Get a map from the index of appearance in a class layout to symbol ID of field.
		 * @param fields The interface elements representing the fields in a class.
		 * @param offsets The offsets of the @p fields, in the same order.
		 * @return A vector which has the field symbols in the order in which they appear in the
		 * layout.
		 */
		std::vector<compiler::helios::SymID> getLayoutIndicesToSymIDs(
			const std::vector<tsh::InterfaceElement>& fields,
			const std::vector<base::Optional<Bytes>>& offsets
		) {
			CORE_ASSERT(fields.size() == offsets.size(), "Input vectors must have the same size.");
			std::vector<usize>                   permutation = offsetsToPermutation(offsets);
			std::vector<compiler::helios::SymID> result;
			result.reserve(permutation.size());

			for (const usize field_idx: permutation)
				result.push_back(fields.at(field_idx).getSymbol());

			return result;
		}

		/**
		 * @brief Source position of the class declaration, used to anchor
		 * diagnostics about the class itself.
		 */
		dia::StablePosition classDiagnosticPosition(
			compiler::helios::SymID class_sym, query::Context& ctx
		) {
			auto class_pst = compiler::helios::maybeSymbolPst(class_sym);
			if (class_pst.has_value()) return class_pst.value().unlock(ctx)->getStablePosition();
			// Class should always have a PST node, even generated one (for now)
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Source position of the field's declaration, used to anchor
		 * diagnostics about its type. Falls back to the class declaration when
		 * the field has no PST node.
		 */
		dia::StablePosition fieldDiagnosticPosition(
			compiler::helios::SymID field_sym, compiler::helios::SymID class_sym, query::Context& ctx
		) {
			auto field_pst = compiler::helios::maybeSymbolPst(field_sym);
			if (field_pst.has_value()) return field_pst.value().unlock(ctx)->getStablePosition();
			return classDiagnosticPosition(class_sym, ctx);
		}

		/**
		 * @brief Result of picking a class layout: offsets in declaration order
		 * plus the overall size and alignment.
		 */
		struct PickedClassLayout final {
			std::vector<base::Optional<Bytes>> offsets;
			Bits                               size;
			Bytes                              alignment;
		};

		PickedClassLayout ducklingPickedLayout(const std::vector<CRef<TypeLayout>>& field_layouts) {
			auto  offsets = alignOffsetsForLayoutVector(field_layouts);
			Bits  total   = offsetsToTotalSize(offsets, field_layouts);
			Bytes align   = maxTypeLayoutAlignmentInVector(field_layouts);
			return PickedClassLayout{
				.offsets   = std::move(offsets),
				.size      = total,
				.alignment = align,
			};
		}

		/**
		 * @brief Computes the C-ABI layout for an `extern("C")` class.
		 *
		 * On any conversion failure the function reports a diagnostic per
		 * offending field and fails the layout query, which aborts compilation
		 * of the offending module.
		 */
		PickedClassLayout cAbiPickedLayout(
			tsh::ClassAbstractType                    class_type,
			const std::vector<tsh::InterfaceElement>& field_elements,
			query::Context&                           ctx
		) {
			if (field_elements.empty()) {
				ctx.logInt(makeBox<compiler::helios::ExternCClassEmptyError>(
					classDiagnosticPosition(class_type.getSymbol(), ctx),
					std::string(compiler::helios::name(class_type.getSymbol()).strView())
				));
				query::throwFailed();
			}

			std::vector<abi::types::AbiTypePtr> abi_fields;
			abi_fields.reserve(field_elements.size());
			bool any_failed = false;

			for (const auto& element: field_elements) {
				const auto& conversion
					= ctx.query<QueryCAbiTypeOf>(element.getType(ctx))->valueOrThrow();

				if (!conversion.has_value()) {
					any_failed = true;
					ctx.logInt(makeBox<compiler::helios::FieldNotCCompatibleError>(
						ctx,
						fieldDiagnosticPosition(element.getSymbol(), class_type.getSymbol(), ctx),
						std::string(compiler::helios::name(element.getSymbol()).strView()),
						element.getType(ctx),
						conversion.error()
					));
					continue;
				}
				abi_fields.emplace_back(base::CRef<abi::types::AbiType>(&conversion.value()));
			}

			if (any_failed) query::throwFailed();

			const abi::layout::ComputedLayout computed
				= abi::layout::computeCLayout(compilerTargetABI(), abi_fields);

			std::vector<base::Optional<Bytes>> result_offsets;
			result_offsets.reserve(field_elements.size());
			for (usize i = 0; i < field_elements.size(); ++i)
				result_offsets.emplace_back(computed.field_offsets[i]);

			return PickedClassLayout{
				.offsets   = std::move(result_offsets),
				.size      = base::bytes2bits(computed.size),
				.alignment = computed.alignment,
			};
		}

		/**
		 * @brief Picks the proper layout for a class: the C-ABI computation
		 * for classes annotated `extern("C")`, otherwise the natural-alignment
		 * layout used everywhere else. Panics on an unknown ABI kind.
		 */
		PickedClassLayout pickClassLayout(
			tsh::ClassAbstractType                    class_type,
			const std::vector<tsh::InterfaceElement>& field_elements,
			const std::vector<CRef<TypeLayout>>&      field_layouts,
			query::Context&                           ctx
		) {
			const compiler::helios::SymbolABI abi = class_type.getABI(ctx);
			variant_match(abi) {
				variant_case_novalue(compiler::helios::DefaultAbi) {
					return ducklingPickedLayout(field_layouts);
				}
				variant_case_novalue(compiler::helios::CAbi) {
					return cAbiPickedLayout(class_type, field_elements, ctx);
				}
			}
			CORE_PANIC("unknown symbol ABI kind");
		}
	}

	TypeLayoutABC::TypeLayoutABC(
		const Bits size, const tsh::SymbolType<> source_type, query::Context& ctx
	):
		  size(size),
		  alignment(computeDefaultAlignment(size)),
		  source_type(source_type),
		  mangled_name(ctx.query<helios::mangler::QueryMangledType>(source_type)->valueOrThrow()) {}

	TypeLayoutABC::TypeLayoutABC(
		Bits size, Bytes alignment, tsh::SymbolType<> source_type, query::Context& ctx
	):
		  size(size),
		  alignment(alignment),
		  source_type(source_type),
		  mangled_name(ctx.query<helios::mangler::QueryMangledType>(source_type)->valueOrThrow()) {}

	StaticArrayTypeLayout::StaticArrayTypeLayout(
		const tsh::StaticArrayAbstractType static_array_type, query::Context& ctx
	):
		  TypeLayoutABC(
			  ctx.query<QuerySymbolTypeLayout>(static_array_type.getElementType())
					  ->valueOrThrow()
					  .getSize()
				  * static_array_type.getSize(),
			  ctx.query<QuerySymbolTypeLayout>(static_array_type.getElementType())
				  ->valueOrThrow()
				  .getAlignment(),
			  tsh::SymbolType<>::withDefaults(static_array_type),
			  ctx
		  ),
		  element_layout(
			  &ctx.query<QuerySymbolTypeLayout>(static_array_type.getElementType())->valueOrThrow()
		  ),
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
			index_to_layout.emplace_back(&ctx.query<QuerySymbolTypeLayout>(type)->valueOrThrow());
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

	struct ClassTypeLayoutConstructionHelper final {
		tsh::AbstractType type;
		/**
		 * The fields of the class, in declaration order.
		 */
		std::vector<tsh::InterfaceElement> field_elements;
		/**
		 * The layouts of the fields, in declaration order.
		 */
		std::vector<CRef<TypeLayout>> field_layouts;
		/**
		 * The offsets of the fields, in declaration order, if not empty.
		 * Note, the offsets are not necessarily increasing.
		 */
		std::vector<base::Optional<Bytes>> field_offsets;
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
		/**
		 * The alignment of the class type, equal to the maximum alignment of its members.
		 * An empty class has alignment 1.
		 */
		Bytes max_alignment;

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

		template<typename T>
		requires std::derived_from<T, tsh::AbstractType>
		ClassTypeLayoutConstructionHelper(const T class_type, query::Context& ctx):
			  type(class_type),
			  field_elements(getFieldsOfInterface(class_type.getInterface(ctx))),
			  field_layouts(getLayoutVector(getElementTypes(field_elements, ctx), ctx)),
			  total_size(0),
			  max_alignment(1) {
			// For class types honor their ABI (C vs natural); pickClassLayout will compute
			// proper offsets/size/alignment. For other aggregate-like types the
			// corresponding constructor (e.g. tuple) will handle layout differently.
			PickedClassLayout picked
				= pickClassLayout(class_type, field_elements, field_layouts, ctx);
			field_offsets           = std::move(picked.offsets);
			layout_idx_to_field_idx = offsetsToPermutation(field_offsets);
			layout_idx_to_sym_id    = getLayoutIndicesToSymIDs(field_elements, field_offsets);
			total_size              = picked.size;
			max_alignment           = picked.alignment;
		}

		ClassTypeLayoutConstructionHelper(
			const tsh::TupleAbstractType tuple_type, query::Context& ctx
		):
			  type(tuple_type),
			  field_elements(getFieldsOfInterface(tuple_type.getInterface(ctx))),
			  field_layouts(getLayoutVector(tuple_type.getComponents(), ctx)),
			  field_offsets(alignOffsetsForLayoutVector(field_layouts)),
			  layout_idx_to_field_idx(offsetsToPermutation(field_offsets)),
			  layout_idx_to_sym_id(getLayoutIndicesToSymIDs(field_elements, field_offsets)),
			  total_size(offsetsToTotalSize(field_offsets, field_layouts)),
			  max_alignment(maxTypeLayoutAlignmentInVector(field_layouts)) {}

		ClassTypeLayoutConstructionHelper(
			const tsh::SliceAbstractType slice_type, query::Context& ctx
		):
			  type(slice_type),
			  field_elements(getFieldsOfInterface(slice_type.getInterface(ctx))),
			  field_layouts(getLayoutVector(getElementTypes(field_elements, ctx), ctx)),
			  field_offsets(alignOffsetsForLayoutVector(field_layouts)),
			  layout_idx_to_field_idx(offsetsToPermutation(field_offsets)),
			  layout_idx_to_sym_id(getLayoutIndicesToSymIDs(field_elements, field_offsets)),
			  total_size(offsetsToTotalSize(field_offsets, field_layouts)),
			  max_alignment(maxTypeLayoutAlignmentInVector(field_layouts)) {}
	};

	ClassTypeLayout::ClassTypeLayout(const tsh::ClassAbstractType class_type, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(class_type, ctx), ctx) {}

	ClassTypeLayout::ClassTypeLayout(const tsh::TupleAbstractType tuple_type, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(tuple_type, ctx), ctx) {}

	ClassTypeLayout::ClassTypeLayout(const tsh::SliceAbstractType slice_type, query::Context& ctx):
		  ClassTypeLayout(ClassTypeLayoutConstructionHelper(slice_type, ctx), ctx) {}

	ClassTypeLayout::ClassTypeLayout(ClassTypeLayoutConstructionHelper&& helper, query::Context& ctx):
		  TypeLayoutABC(
			  helper.total_size,
			  helper.max_alignment,
			  tsh::SymbolType<>::withDefaults(helper.type),
			  ctx
		  ),
		  num_sub_layouts(helper.layout_idx_to_field_idx.size()),
		  layout_idx_to_sym_id(std::move(helper).layout_idx_to_sym_id) {
		// Fill out the sym_id_to_offset map based on the subsequent field symbols and their offsets.
		for (u32 i = 0; i < num_sub_layouts; i++) {
			sym_id_to_offset.put(
				layout_idx_to_sym_id.at(i),
				helper.field_offsets.at(helper.layout_idx_to_field_idx.at(i))
			);
		}
		for (const auto& ie: helper.field_elements) {
			// Fill the holes left by empty elements.
			if (const auto sym_id = ie.getSymbol(); !sym_id_to_offset.contains(sym_id))
				sym_id_to_offset.put(sym_id, {});
		}
		// Fill out the sym_id_to_layout_idx map as the inverse of layout_idx_to_sym_id.
		for (u32 i = 0; i < num_sub_layouts; i++) {
			// Fill out what we know.
			sym_id_to_layout_idx.put(layout_idx_to_sym_id.at(i), i);
		}
		for (const auto& ie: helper.field_elements) {
			// Fill the holes left by empty elements.
			if (const auto sym_id = ie.getSymbol(); !sym_id_to_layout_idx.contains(sym_id))
				sym_id_to_layout_idx.put(sym_id, {});
		}
		layout_idx_to_layout.reserve(num_sub_layouts);
		for (u32 i = 0; i < num_sub_layouts; i++) {
			// Fill out the map based on the field layouts of subsequent layout components.
			layout_idx_to_layout.push_back(
				helper.field_layouts.at(helper.layout_idx_to_field_idx.at(i))
			);
		}
	}

	std::string ClassTypeLayout::toStringDefinition(
		query::Context& ctx, const bool recursive, const u32 indent
	) const {
		const tsh::SymbolType<> class_type = getSourceType();
		std::stringstream       ss{};

		// Display the class header and components
		ss << getIndent(indent) << class_type.toString() << " {\n";
		for (const auto field_sym_id: layout_idx_to_sym_id) {
			const base::Optional<Bytes> field_offset = getOffsetOfFieldSymbol(field_sym_id);
			const tsh::SymbolType<>     field_type
				= ctx.query<helios::QueryTypeOfSymbol>(field_sym_id)->valueOrThrow();
			const auto field_layout
				= CRef<TypeLayout>(&ctx.query<QuerySymbolTypeLayout>(field_type)->valueOrThrow());
			if (recursive)
				ss << field_layout->toStringDefinition(ctx, recursive, indent + 1);
			else
				ss << getIndent(indent + 1) << field_layout->toStringIdentification();
			// Display the offset
			ss << " @ " << (field_offset ? base::toString(field_offset.value()) : "nowhere")
			   << "\n";
		}

		// Display the total size
		ss << getIndent(indent) << "} : " << base::toString(getSize());

		return ss.str();
	}

	PointerTypeLayout::PointerTypeLayout(
		const tsh::PointerAbstractType pointer_type, query::Context& ctx
	):
		  TypeLayoutABC(POINTER_SIZE, tsh::SymbolType<>::withDefaults(pointer_type), ctx),
		  pointee_type(pointer_type.getPointee()),
		  pointer_kind(PointerKind::SinglePointer) {}

	PointerTypeLayout::PointerTypeLayout(
		const tsh::ManyPointerAbstractType pointer_type, query::Context& ctx
	):
		  TypeLayoutABC(POINTER_SIZE, tsh::SymbolType<>::withDefaults(pointer_type), ctx),
		  pointee_type(pointer_type.getPointee()),
		  pointer_kind(PointerKind::ManyPointer) {}

	PointerTypeLayout::PointerTypeLayout(
		const tsh::CPointerAbstractType pointer_type, query::Context& ctx
	):
		  TypeLayoutABC(POINTER_SIZE, tsh::SymbolType<>::withDefaults(pointer_type), ctx),
		  pointee_type(pointer_type.getPointee()),
		  pointer_kind(PointerKind::CPointer) {
		auto& pointee_cabi_type
			= ctx.query<QueryCAbiTypeOf>(pointer_type.getPointee())->valueOrThrow();
		if (not pointee_cabi_type.has_value()) {
			ctx.logInt(
				makeBox<dia::PlaceholderError>("Invalid cptr type.", pointee_cabi_type.error())
			);
			query::throwFailed();
		}
	}

	PointerTypeLayout::PointerTypeLayout(const tsh::SymbolType<> symbol_type, query::Context& ctx):
		  TypeLayoutABC(POINTER_SIZE, symbol_type, ctx),
		  pointee_type(tsh::SymbolType<>::withDefaults(symbol_type.getType())),
		  pointer_kind(PointerKind::SinglePointer) {
		CORE_ASSERT(
			symbol_type.getRefKind() != tsh::ReferenceKind::Direct,
			"Construction of pointer layout from symbol type "
			"without reference indirection is forbidden."
		);
	}

	CRef<TypeLayout> PointerTypeLayout::getPointee(query::Context& ctx) const {
		CORE_ASSERT(pointee_type.has_value(), "Untyped pointer layout has no pointee.");
		return &ctx.query<QuerySymbolTypeLayout>(*pointee_type)->valueOrThrow();
	}

	Bits TypeLayout::getSize() const { return VISIT(variant, l, return l.getSize()); }

	Bytes TypeLayout::getAlignment() const { return VISIT(variant, l, return l.getAlignment()); }

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
