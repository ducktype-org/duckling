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
#include <base/bits_and_bytes.hpp>

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
		Bits size;

		/**
		 * @brief The source type of a memory layout.
		 */
		tsh::TypeInfo source_type;

	public:
		// This definition is necessary for default definitions in deriving classes.
		bool operator<=>(const TypeLayoutABC& other) const = default;

		/**
		 * @brief Get the total size of a layout, in bits.
		 * @return The total size of a layout, in bits.
		 */
		[[nodiscard]]
		Bits getSize() const {
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

		/**
		 * @brief Get a string describing the layout in a human-friendly format.
		 * @param ctx The query context for fetching layouts of components in composite layouts.
		 * @param recursive Whether the layout string should contain full layout strings of
		 * composite component types. Setting this to `false` will result in the use of IDs instead.
		 * @param indent The indent at which to print. Mostly used internally for recursive prints.
		 * @return A string describing the layout.
		 */
		[[nodiscard]]
		virtual std::string
			toStringDefinition(query::Context& ctx, bool recursive = true, u32 indent = 0) const
			= 0;

		/**
		 * @brief Get a relatively short string identifying the type layout.
		 * @return A string identifying the type layout.
		 */
		[[nodiscard]]
		virtual std::string toStringIdentification() const {
			return "Layout of " + source_type.toString() + " : " + std::to_string(getSize());
		}

		virtual ~TypeLayoutABC() = default;

	protected:
		TypeLayoutABC(Bits size, tsh::TypeInfo source_type): size(size), source_type(source_type) {}

		[[nodiscard]]
		static auto getIndent(u32 indent) {
			return std::string(indent, '\t');
		}
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
		EmptyTypeLayout(tsh::UnitInfo unit_info): TypeLayoutABC(Bits(0), unit_info) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, u32 indent) const override {
			return getIndent(indent) + "{} : " + std::to_string(getSize());
		}
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

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, u32 indent) const override {
			return getIndent(indent) + "i" + std::to_string(usize(getSize())) + " : "
			     + std::to_string(getSize());
		}
	};

	/**
	 * @brief Layout of a type that has float-like low level behaviour.
	 */
	class FloatTypeLayout: public TypeLayoutABC {
	public:
		FloatTypeLayout(tsh::FloatInfo float_info):
			  TypeLayoutABC(float_info.getSize(), float_info) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, u32 indent) const override {
			return getIndent(indent) + "f" + std::to_string(usize(getSize())) + " : "
			     + std::to_string(getSize());
		}
	};

	/**
	 * @brief Layout of a variant type.
	 */
	class VariantTypeLayout: public TypeLayoutABC {
		Bytes tag_offset;
		Bits  tag_size;
		Bytes data_offset;
		Bits  data_size;

		base::Map<tsh::TypeInfo, usize> type_to_index;
		std::vector<tsh::TypeInfo>      index_to_type;

		// Delegate constructor.
		VariantTypeLayout(struct VariantTypeLayoutConstructionHelper helper);

	public:
		VariantTypeLayout(tsh::VariantInfo variant_info, query::Context& ctx);

		/**
		 * @return The offset of the discriminating tag of this variant, in bytes.
		 */
		[[nodiscard]]
		Bytes getTagOffset() const {
			return tag_offset;
		}

		/**
		 * @return The size of the tag itself, in bits.
		 */
		[[nodiscard]]
		Bits getTagSize() const {
			return tag_size;
		}

		/**
		 * @return The offset of the actual data held by this variant, in bytes, up to 8.
		 */
		[[nodiscard]]
		Bytes getDataOffset() const {
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

		[[nodiscard]]
		std::string
			toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const override;
	};

	/**
	 * @brief Layout of a tuple type.
	 */
	class TupleTypeLayout: public TypeLayoutABC {
		/**
		 * @brief The component offsets, in bytes.
		 *
		 * @note Not necessarily increasing. These offsets are given in the order of the components
		 * in the source tuple type. This order may not be preserved in the layout.
		 */
		std::vector<Bytes> component_offsets;

		/**
		 * @brief The component indices of the components in the original tuple type, sorted by
		 * their order of appearance (offset) in the layout.
		 */
		std::vector<usize> offset_idx_to_component_idx;

		// Delegate constructor.
		TupleTypeLayout(struct TupleTypeLayoutConstructionHelper&& helper);

	public:
		TupleTypeLayout(tsh::TupleInfo tuple_info, query::Context& ctx);

		/**
		 * @return The full list of component offsets, in bytes.
		 *
		 * @note Not necessarily increasing. These offsets are given in the order of the components
		 * in the source tuple type. This order may not be preserved in the layout.
		 */
		[[nodiscard]]
		const std::vector<Bytes>& getComponentOffsets() const {
			return component_offsets;
		}

		/**
		 * @param index The index of a component in the original tuple type.
		 * @return The offset of the component corresponding to the given index, in bytes.
		 */
		[[nodiscard]]
		Bytes getComponentOffset(usize index) const {
			return getComponentOffsets()[index];
		}

		[[nodiscard]]
		std::string
			toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const override;
	};

	/**
	 * @brief Layout of class type.
	 *
	 * @todo Add vtable support.
	 * @todo Add layout of base classes.
	 */
	class ClassTypeLayout: public TypeLayoutABC {
		base::Map<compiler::helios::SymID, Bytes> field_offsets;
		/**
		 * @brief A mapping of the order of appearance in the layout to the symbol of the field.
		 */
		std::vector<compiler::helios::SymID> offset_idx_to_sym_id;

		// Delegate constructor.
		ClassTypeLayout(struct ClassTypeLayoutConstructionHelper&& helper);

	public:
		ClassTypeLayout(tsh::ClassInfo class_info, query::Context& ctx);

		/**
		 * @return The full dictionary of field offsets, in bytes.
		 */
		[[nodiscard]]
		const base::Map<compiler::helios::SymID, Bytes>& getFieldOffsets() const {
			return field_offsets;
		}

		/**
		 * @param symbol The symbol of a field.
		 * @return The offset of the field corresponding to the given symbol, in bytes.
		 */
		[[nodiscard]]
		Bytes getFieldOffset(compiler::helios::SymID symbol) const {
			return getFieldOffsets().at(symbol);
		}

		[[nodiscard]]
		std::string
			toStringDefinition(query::Context& ctx, bool recursive, u32 indent) const override;
	};

	/**
	 * @brief Layout of a functional object or pointer type.
	 */
	class FunctionalTypeLayout: public TypeLayoutABC {
	public:
		// @TODO: Add support for function objects
		FunctionalTypeLayout(tsh::FunctionInfo function_info):
			  TypeLayoutABC(POINTER_SIZE, function_info) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, u32 indent) const override {
			return getIndent(indent) + "Functional : " + std::to_string(getSize());
		}
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

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, u32 indent) const override {
			return getIndent(indent) + "Pointer to " + getSourceType().toString() + " : "
			     + std::to_string(getSize());
		}
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
	 *
	 * @note Use operator() when matching against the variant's options.
	 */
	class TypeLayout: public TypeLayoutDirectVariant {
	public:
		// Use when matching against the variant's options.
		TypeLayoutDirectVariant& operator()() { return *this; }

		// Use when matching against the variant's options.
		const TypeLayoutDirectVariant& operator()() const { return *this; }

		using TypeLayoutDirectVariant::TypeLayoutDirectVariant;

		/**
		 * @copydoc TypeLayoutABC::getSize
		 */
		[[nodiscard]]
		Bits getSize() const;

		/**
		 * @copydoc TypeLayoutABC::getSourceType
		 */
		[[nodiscard]]
		tsh::TypeInfo getSourceType() const;

		/**
		 * @copydoc TypeLayoutABC::toStringDefinition
		 */
		[[nodiscard]]
		std::string
			toStringDefinition(query::Context& ctx, bool recursive = true, u32 indent = 0) const;

		/**
		 * @copydoc TypeLayoutABC::toStringIdentification
		 */
		[[nodiscard]]
		std::string toStringIdentification() const;
	};
}
