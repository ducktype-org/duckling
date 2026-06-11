#include <abi/layout/compute_c_layout.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <algorithm>

namespace abi::layout {

	namespace {
		/**
		 * @brief Rounds `offset` up to the nearest multiple of `alignment`.
		 * `alignment` must be a positive integer; it does not need to be a
		 * power of two but for natural C alignments it always will be.
		 */
		Bytes roundUpToAlignment(Bytes offset, Bytes alignment) {
			const auto off = usize(offset);
			const auto al  = usize(alignment);
			CORE_ASSERT(al > 0, "alignment must be positive");
			const usize remainder = off % al;
			if (remainder == 0) return offset;
			return Bytes(off + (al - remainder));
		}
	}

	SizeAlign sizeAlignOf(const TargetABI& target, const type_system::AbiType& t) {
		SizeAlign out{ .size = Bytes(0), .alignment = Bytes(1) };

		variant_match(t.value) {
			variant_case(type_system::IntType, i) {
				CORE_ASSERT(
					usize(i.width_bits) % 8 == 0, "integer width must be a multiple of 8 bits"
				);
				out.size      = Bytes(usize(i.width_bits) / 8);
				out.alignment = target.data_layout.naturalAlignmentForIntWidth(i.width_bits);
			}
			variant_case(type_system::FloatType, f) {
				// Size and alignment come straight from the target's float table.
				// The converter rejects widths the target does not list, so a
				// missing entry here is a bug rather than a user error.
				const base::Optional<SizeAlign> found
					= target.data_layout.float_layouts.atMaybeCopy(f.width_bits);
				CORE_ASSERT(
					found.has_value(),
					"float width not representable on this target; reject it in the converter: ",
					usize(f.width_bits)
				);
				out.size      = found.value().size;
				out.alignment = found.value().alignment;
			}
			variant_case_novalue(type_system::BoolType) {
				// C `_Bool`: a 1-byte ABI type on every supported target.
				out.size      = Bytes(1);
				out.alignment = Bytes(1);
			}
			variant_case_novalue(type_system::CharType) {
				// C `char`: a 1-byte integer; signedness does not affect layout.
				out.size      = Bytes(1);
				out.alignment = Bytes(1);
			}
			variant_case_novalue(type_system::PointerType) {
				out.size      = target.data_layout.pointer_size;
				out.alignment = target.data_layout.pointer_alignment;
			}
			variant_case(type_system::ArrayType, a) {
				CORE_ASSERT(
					a.count > 0, "zero-length arrays are not legal in C ABI; filter on caller side"
				);
				SizeAlign element = sizeAlignOf(target, *a.element);
				out.size          = element.size * a.count;
				out.alignment     = element.alignment;
			}
			variant_case(type_system::StructType, s) {
				CORE_ASSERT(
					!s.fields.empty(), "empty structs are not legal in C ABI; filter on caller side"
				);
				ComputedLayout sub = computeCLayout(target, s.fields);
				out.size           = sub.size;
				out.alignment      = sub.alignment;
			}
			variant_case(type_system::OpaqueType, o) {
				out.size      = o.size;
				out.alignment = o.alignment;
			}
		}

		CORE_ASSERT(usize(out.size) > 0, "zero-sized field in C layout; filter on caller side");

		return out;
	}

	ComputedLayout computeCLayout(
		const TargetABI& target, const std::vector<type_system::Field>& fields
	) {
		CORE_ASSERT(!fields.empty(), "empty structs are not legal in C ABI; filter on caller side");

		ComputedLayout out;
		out.field_offsets.reserve(fields.size());

		Bytes cursor(0);
		Bytes max_align(1);

		for (const auto& f: fields) {
			SizeAlign sa = sizeAlignOf(target, *f.type);
			cursor       = roundUpToAlignment(cursor, sa.alignment);
			out.field_offsets.push_back(cursor);
			cursor    = cursor + sa.size;
			max_align = Bytes(std::max(usize(max_align), usize(sa.alignment)));
		}

		out.alignment = max_align;
		out.size      = roundUpToAlignment(cursor, max_align);
		return out;
	}

}
