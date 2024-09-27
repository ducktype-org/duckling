#pragma once

#include "size_constants.hpp"

#include <base/box.hpp>
#include <base/ref.hpp>
#include <typesystem/higher/type_info.hpp>
#include <typesystem/higher/types.hpp>
#include <typesystem/higher/type_desc.hpp>

#include <query_framework/query_int.hpp>

#include <base/variant.hpp>
#include <base/maps.hpp>

/**
 * @brief The namespace of all definitions of the Lower Type System.
 * Short for "Type System: Low(er)".
 */
namespace tsl {
	/**
	 * @brief The abstract base class of a Type Layout object.
	 */
	class TypeLayoutABC {
		/**
		 * @brief The runtime contiguous size of a type's memory layout.
		 */
		usize size;

		/**
		 * @brief The source type of a memory layout.
		 */
		tsh::TypeInfo source_type;

	public:
		// This definition is necessary for default definitions in deriving classes.
		bool operator==(const TypeLayoutABC& other) const = default;

		/**
		 * @brief Get the total size of a layout, in bits.
		 * @return The total size of a layout, in bits.
		 */
		[[nodiscard]]
		usize getSize() const {
			return size;
		}

		/**
		 * @brief Get the source type of a layout.
		 * @return The source type of a layout.
		 */
		[[nodiscard]]
		tsh::TypeInfo getSourceType() const {
			return source_type;
		}

	protected:
		TypeLayoutABC(usize size, tsh::TypeInfo source_type):
			  size(size),
			  source_type(source_type) {}
	};

	class TypeLayout;

	/**
	 * @brief Layout of a type that does not require any representation in memory.
	 *
	 * @note This is only viable for Unit, as it only has a single value,
	 * which carries no information. While Void also carries no information, it has no values
	 * whatsoever, so considering a layout for it is invalid and should not be "useful".
	 */
	class EmptyTypeLayout: public TypeLayoutABC {
	public:
		EmptyTypeLayout(tsh::UnitInfo unit_info): TypeLayoutABC(0, unit_info) {}
	};

	/**
	 * @brief Layout of a type that has integral-like low level behaviour.
	 *
	 * Valid candidates include, of course, integers, but also bytes, bools, and characters.
	 */
	class IntegralTypeLayout: public TypeLayoutABC {
	public:
		IntegralTypeLayout(tsh::ByteInfo byte_info): TypeLayoutABC(tsl::BYTE_SIZE, byte_info) {}

		IntegralTypeLayout(tsh::BoolInfo bool_info): TypeLayoutABC(tsl::BOOL_SIZE, bool_info) {}

		IntegralTypeLayout(tsh::CharInfo char_info): TypeLayoutABC(tsl::CHAR_SIZE, char_info) {}

		IntegralTypeLayout(tsh::IntegralInfo integral_info):
			  TypeLayoutABC(integral_info.getSize(), integral_info) {}
	};

	/**
	 * @brief Layout of a type that has float-like low level behaviour.
	 */
	class FloatTypeLayout: public TypeLayoutABC {
	public:
		FloatTypeLayout(tsh::FloatInfo float_info):
			  TypeLayoutABC(float_info.getSize(), float_info) {}
	};

	/**
	 * @brief Layout of a variant type.
	 */
	class VariantTypeLayout: public TypeLayoutABC {
		usize tag_offset;
		usize tag_size;
		usize data_offset;

		base::Map<tsh::TypeInfo, usize> type_to_index;
		std::vector<tsh::TypeInfo>      index_to_type;

	public:
		VariantTypeLayout(tsh::VariantInfo variant_info, query::Context& ctx);

		/**
		 * @return The offset of the discriminating tag of this variant, in bytes.
		 */
		[[nodiscard]]
		usize getTagOffset() const {
			return tag_offset;
		}

		/**
		 * @return The size of the tag itself, in bits.
		 */
		[[nodiscard]]
		usize getTagSize() const {
			return tag_size;
		}

		/**
		 * @return The offset of the actual data held by this variant, in bytes, up to 8.
		 */
		[[nodiscard]]
		usize getDataOffset() const {
			return data_offset;
		}

		/**
		 * @param key One of the options of the variant.
		 * @return The index (tag value) of the given option.
		 */
		[[nodiscard]]
		usize getIndexOfType(tsh::TypeInfo key) const {
			return type_to_index.at(key);
		}

		/**
		 * @param index An in-bounds index (tag value) of the variant.
		 * @return The type corresponding to that index.
		 */
		[[nodiscard]]
		tsh::TypeInfo getTypeOfIndex(usize index) const {
			return index_to_type[index];
		}
	};

	/**
	 * @brief Layout of a tuple type.
	 */
	class TupleTypeLayout: public TypeLayoutABC {
		/**
		 * @brief The component offsets, in bytes.
		 */
		std::vector<usize> component_offsets;

		// Delegate constructor.
		TupleTypeLayout(struct TupleTypeLayoutConstructionHelper&& helper);

	public:
		TupleTypeLayout(tsh::TupleInfo tuple_info, query::Context& ctx);

		/**
		 * @return The full list of component offsets, in bytes.
		 */
		[[nodiscard]]
		const std::vector<usize>& getComponentOffsets() const {
			return component_offsets;
		}

		/**
		 * @param index The index of a component.
		 * @return The offset of the component corresponding to the given index, in bytes.
		 */
		[[nodiscard]]
		usize getComponentOffset(usize index) const {
			return component_offsets[index];
		}
	};

	/**
	 * @brief Layout of class type.
	 *
	 * @todo Add vtable support.
	 */
	class ClassTypeLayout: public TypeLayoutABC {
		base::Map<compiler::helios::SymID, usize> field_offsets;

		// Delegate constructor.
		ClassTypeLayout(const struct ClassTypeLayoutConstructionHelper& helper);

	public:
		ClassTypeLayout(tsh::ClassInfo class_info, query::Context& ctx);

		/**
		 * @return The full dictionary of field offsets, in bytes.
		 */
		[[nodiscard]]
		const base::Map<compiler::helios::SymID, usize>& getFieldOffsets() const {
			return field_offsets;
		}

		/**
		 * @param symbol The symbol of a field.
		 * @return The offset of the field corresponding to the given symbol, in bytes.
		 */
		[[nodiscard]]
		usize getFieldOffset(compiler::helios::SymID symbol) const {
			return field_offsets.at(symbol);
		}
	};

	/**
	 * @brief Layout of a functional object or pointer type.
	 */
	class FunctionalTypeLayout: public TypeLayoutABC {
	public:
		// @TODO: Add support for function objects
		FunctionalTypeLayout(tsh::FunctionInfo function_info):
			  TypeLayoutABC(POINTER_SIZE, function_info) {}
	};

	/**
	 * @brief Layout of a type that has pointer-like low level behaviour.
	 */
	class PointerTypeLayout: public TypeLayoutABC {
		MBox<TypeLayout> pointee;

	public:
		PointerTypeLayout(const PointerTypeLayout& other):
			  TypeLayoutABC(other.getSize(), other.getSourceType()),
			  pointee(other.pointee ? box<TypeLayout>(*other.pointee) : MBox<TypeLayout>{}) {}

		Ref<TypeLayout> operator->() { return pointee.toOpt().value(); }

		Ref<TypeLayout> getPointee() { return operator->(); }

		bool hasPointee() { return pointee; }

		/**
		 * @brief Construct a PointerLayout for a RawPointer.
		 * @param raw_pointer_info The source RawPointer.
		 */
		PointerTypeLayout(tsh::RawPointerInfo raw_pointer_info):
			  TypeLayoutABC(POINTER_SIZE, raw_pointer_info),
			  pointee() {}

		/**
		 * @brief Construct a PointerLayout from a typed Pointer.
		 * @param pointer_info The source typed Pointer.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::PointerInfo pointer_info, query::Context& ctx);

		// There is NO constructor from ReferenceInfo because
		// ReferenceInfo will be soon removed. @TODO: remove this comment.
	};

	using TypeLayoutDirectVariant = std::variant<
		EmptyTypeLayout,
		IntegralTypeLayout,
		FloatTypeLayout,
		VariantTypeLayout,
		TupleTypeLayout,
		ClassTypeLayout,
		FunctionalTypeLayout,
		PointerTypeLayout>;

	/**
	 * @brief The ADT representing the layout of a type.
	 */
	class TypeLayout: public TypeLayoutDirectVariant {
	public:
		TypeLayoutDirectVariant& operator()() { return *this; }

		const TypeLayoutDirectVariant& operator()() const { return *this; }

		using TypeLayoutDirectVariant::TypeLayoutDirectVariant;

		[[nodiscard]]
		usize getSize() const {
			variant_match((*this)()) {
				variant_case(EmptyTypeLayout, l) { return l.getSize(); }
				variant_case(IntegralTypeLayout, l) { return l.getSize(); }
				variant_case(FloatTypeLayout, l) { return l.getSize(); }
				variant_case(VariantTypeLayout, l) { return l.getSize(); }
				variant_case(ClassTypeLayout, l) { return l.getSize(); }
				variant_case(FunctionalTypeLayout, l) { return l.getSize(); }
				variant_case(PointerTypeLayout, l) { return l.getSize(); }
			}
			RIFT_PANIC("Unmatched type layout.");
		}

		[[nodiscard]]
		tsh::TypeInfo getSourceType() const {
			variant_match((*this)()) {
				variant_case(EmptyTypeLayout, l) { return l.getSourceType(); }
				variant_case(IntegralTypeLayout, l) { return l.getSourceType(); }
				variant_case(FloatTypeLayout, l) { return l.getSourceType(); }
				variant_case(VariantTypeLayout, l) { return l.getSourceType(); }
				variant_case(ClassTypeLayout, l) { return l.getSourceType(); }
				variant_case(FunctionalTypeLayout, l) { return l.getSourceType(); }
				variant_case(PointerTypeLayout, l) { return l.getSourceType(); }
			}
			RIFT_PANIC("Unmatched type layout.");
		}
	};
}
