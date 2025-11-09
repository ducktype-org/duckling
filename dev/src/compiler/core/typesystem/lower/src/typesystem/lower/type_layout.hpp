#pragma once

#include "size_constants.hpp"

#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/higher/types.hpp>

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <variant>

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
		tsh::AbstractType source_type;

		/**
		 * @brief The kind of reference indirection applied to the source type.
		 *
		 * This usually has the value ReferenceKind::DIRECT, but will be either REF or BOX for when
		 * a layout is created for a SymbolType with an indirection.
		 */
		tsh::ReferenceKind reference_kind;

	public:
		// This definition is necessary for default definitions in deriving classes.
		bool operator==(const TypeLayoutABC& other) const = default;

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
		tsh::AbstractType getSourceType() const {
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
		virtual std::string toStringDefinition(
			query::Context& ctx, bool recursive = true, u32 indent = 0
		) const
			= 0;

		/**
		 * @brief Get a relatively short string identifying the type layout.
		 * @return A string identifying the type layout.
		 */
		[[nodiscard]]
		virtual std::string toStringIdentification() const {
			return "Layout of "
			     + std::string(
					   reference_kind == tsh::ReferenceKind::Direct ? ""
					   : reference_kind == tsh::ReferenceKind::Ref  ? "ref "
																	: "box "
				 )
			     + source_type.toString() + " : " + base::toString(getSize());
		}

		virtual ~TypeLayoutABC() = default;

	protected:
		TypeLayoutABC(
			const Bits               size,
			const tsh::AbstractType  source_type,
			const tsh::ReferenceKind reference_kind = tsh::ReferenceKind::Direct
		):
			  size(size),
			  source_type(source_type),
			  reference_kind(reference_kind) {}

		[[nodiscard]]
		static auto getIndent(const u32 indent) {
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
	class EmptyTypeLayout final: public TypeLayoutABC {
	public:
		explicit EmptyTypeLayout(const tsh::UnitAbstractType unit_type):
			  TypeLayoutABC(Bits(0), unit_type) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "{} : " + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout of a type that has integral-like low level behaviour.
	 *
	 * Valid candidates include, of course, integers, but also bytes, bools, and characters.
	 */
	class IntegralTypeLayout final: public TypeLayoutABC {
	public:
		explicit IntegralTypeLayout(const tsh::ByteAbstractType byte_type):
			  TypeLayoutABC(BYTE_SIZE, byte_type) {}

		explicit IntegralTypeLayout(const tsh::BoolAbstractType bool_type):
			  TypeLayoutABC(BOOL_SIZE, bool_type) {}

		explicit IntegralTypeLayout(const tsh::CharAbstractType char_type):
			  TypeLayoutABC(CHAR_SIZE, char_type) {}

		explicit IntegralTypeLayout(const tsh::IntegralAbstractType integral_type):
			  TypeLayoutABC(integral_type.getSize(), integral_type) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "i" + std::to_string(usize(getSize())) + " : "
			     + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout of a type that has float-like low level behaviour.
	 */
	class FloatTypeLayout final: public TypeLayoutABC {
	public:
		explicit FloatTypeLayout(const tsh::FloatAbstractType float_type):
			  TypeLayoutABC(float_type.getSize(), float_type) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "f" + std::to_string(usize(getSize())) + " : "
			     + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout of the string type.
	 * It is similar to DynamicArrayTypeLayout, but intentionally implemented separately.
	 */
	class StringTypeLayout final: public TypeLayoutABC {
		/**
		 * The string type consists of four parts of information:
		 * -# Pointer to the start of data
		 * -# Offset of the end of data wrt. the pointer to its start
		 * -# Offset of the start of reserved memory
		 * -# Offset of the end of reserved memory
		 * The pointer and offsets are arranged in this exact order in memory.
		 */
		static constexpr auto OFFSET_SIZE = Bytes(8);

	public:
		explicit StringTypeLayout(const tsh::StringAbstractType string_type):
			  TypeLayoutABC(POINTER_SIZE + base::bytes2bits(OFFSET_SIZE) * 3, string_type) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "string : " + base::toString(getSize());
		}

		/**
		 * @return The offset of the end of data offset
		 */
		[[nodiscard]]
		Bytes getEndOfDataOffsetPosition() const {
			return POINTER_SIZE_BYTES;
		}

		/**
		 * @return The offset of the start of reserved memory offset
		 */
		[[nodiscard]]
		Bytes getStartOfMemoryOffsetPosition() const {
			return POINTER_SIZE_BYTES + OFFSET_SIZE;
		}

		/**
		 * @return The offset of the end of reserved memory offset
		 */
		[[nodiscard]]
		Bytes getEndOfMemoryOffsetPosition() const {
			return POINTER_SIZE_BYTES + OFFSET_SIZE * 2;
		}
	};

	/**
	 * @brief Layout of a dynamic array type.
	 */
	class DynamicArrayTypeLayout final: public TypeLayoutABC {
		/**
		 * The dynamic array type consists of four parts of information:
		 * -# Pointer to the start of data
		 * -# Offset of the end of data
		 * -# Offset of the start of reserved memory
		 * -# Offset of the end of reserved memory
		 * The pointer and offsets are arranged in this exact order in memory.
		 */
		static constexpr auto OFFSET_SIZE = Bytes(8);

	public:
		DynamicArrayTypeLayout(tsh::DynamicArrayAbstractType dynamic_array_type, query::Context& ctx);

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override;

		/**
		 * @return The offset of the end of data offset
		 */
		[[nodiscard]]
		Bytes getEndOfDataOffsetPosition() const {
			return POINTER_SIZE_BYTES;
		}

		/**
		 * @return The offset of the start of reserved memory offset
		 */
		[[nodiscard]]
		Bytes getStartOfMemoryOffsetPosition() const {
			return POINTER_SIZE_BYTES + OFFSET_SIZE;
		}

		/**
		 * @return The offset of the end of reserved memory offset
		 */
		[[nodiscard]]
		Bytes getEndOfMemoryOffsetPosition() const {
			return POINTER_SIZE_BYTES + OFFSET_SIZE * 2;
		}
	};

	/**
	 * @brief Layout of a variant type.
	 */
	class VariantTypeLayout final: public TypeLayoutABC {
		Bytes tag_offset;
		Bits  tag_size;
		Bytes data_offset;
		Bits  data_size;

		base::Map<tsh::SymbolType<>, usize> type_to_index;
		std::vector<tsh::SymbolType<>>      index_to_type;

		// Delegate constructor.
		explicit VariantTypeLayout(struct VariantTypeLayoutConstructionHelper helper);

	public:
		VariantTypeLayout(tsh::VariantAbstractType variant_type, query::Context& ctx);

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
		usize getIndexOfType(const tsh::SymbolType<> key) const {
			return type_to_index.at(key);
		}

		/**
		 * @param index An in-bounds index (tag value) of the variant.
		 * @return The type corresponding to that index.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getTypeOfIndex(const usize index) const {
			return index_to_type[index];
		}

		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive, u32 indent)
			const override;
	};

	/**
	 * @brief Layout of a tuple type.
	 */
	class TupleTypeLayout final: public TypeLayoutABC {
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
		explicit TupleTypeLayout(struct TupleTypeLayoutConstructionHelper&& helper);

	public:
		TupleTypeLayout(tsh::TupleAbstractType tuple_type, query::Context& ctx);

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
		Bytes getComponentOffset(const usize index) const {
			return getComponentOffsets()[index];
		}

		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive, u32 indent)
			const override;
	};

	/**
	 * @brief Layout of class type.
	 *
	 * @todo Add vtable support.
	 * @todo Add layout of base classes.
	 */
	class ClassTypeLayout final: public TypeLayoutABC {
		base::Map<compiler::helios::SymID, Bytes> field_offsets;
		/**
		 * @brief A mapping of the order of appearance in the layout to the symbol of the field.
		 */
		std::vector<compiler::helios::SymID> offset_idx_to_sym_id;

		// Delegate constructor.
		explicit ClassTypeLayout(struct ClassTypeLayoutConstructionHelper&& helper);

	public:
		ClassTypeLayout(tsh::ClassAbstractType class_type, query::Context& ctx);

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
		Bytes getFieldOffset(const compiler::helios::SymID symbol) const {
			return getFieldOffsets().at(symbol);
		}

		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive, u32 indent)
			const override;
	};

	/**
	 * @brief Layout of a functional object or pointer type.
	 */
	class FunctionalTypeLayout final: public TypeLayoutABC {
	public:
		// @TODO: Add support for function objects
		explicit FunctionalTypeLayout(const tsh::FunctionAbstractType function_type):
			  TypeLayoutABC(POINTER_SIZE, function_type) {}

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "Functional : " + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout of a type that has pointer-like low level behaviour.
	 */
	class PointerTypeLayout final: public TypeLayoutABC {
		// The layout of the pointee type.
		// Since a pointer may be untyped, the layout of the pointee may be unknown.
		// Hence, the use of a nullable box.
		MBox<TypeLayout> pointee;

	public:
		PointerTypeLayout(const PointerTypeLayout& other):
			  TypeLayoutABC(other.getSize(), other.getSourceType()),
			  pointee(other.pointee ? makeBox<TypeLayout>(*other.pointee) : MBox<TypeLayout>{}) {}

		PointerTypeLayout& operator=(PointerTypeLayout&& other) noexcept {
			TypeLayoutABC::operator=(other);
			pointee = std::move(other.pointee);
			return *this;
		}

		[[nodiscard]]
		Ref<TypeLayout> getPointee() const {
			return pointee.toOpt().value();
		}

		[[nodiscard]]
		bool hasPointee() const {
			return pointee;
		}

		/**
		 * @brief Construct a PointerLayout for a RawPointer.
		 * @param raw_pointer_type The source RawPointer.
		 */
		explicit PointerTypeLayout(const tsh::RawPointerAbstractType raw_pointer_type):
			  TypeLayoutABC(POINTER_SIZE, raw_pointer_type),
			  pointee() {}

		/**
		 * @brief Construct a PointerLayout from a typed Pointer.
		 * @param pointer_type The source typed Pointer.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::PointerAbstractType pointer_type, query::Context& ctx);

		/**
		 * @brief Construct a PointerLayout from a SymbolType, provided that it is not DIRECT.
		 * @param symbol_type A type with reference indirection, i.e. not DIRECT reference kind.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::SymbolType<> symbol_type, query::Context& ctx);

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "Pointer to " + getSourceType().toString() + " : "
			     + base::toString(getSize());
		}
	};

	using TypeLayoutDirectVariant = std::variant<
		EmptyTypeLayout,
		IntegralTypeLayout,
		FloatTypeLayout,
		VariantTypeLayout,
		TupleTypeLayout,
		DynamicArrayTypeLayout,
		ClassTypeLayout,
		FunctionalTypeLayout,
		PointerTypeLayout,
		StringTypeLayout>;

	/**
	 * @brief The ADT representing the layout of a type.
	 *
	 * @note Use getVariant() when matching against the variant's options.
	 */
	class TypeLayout final {
	public:
		TypeLayoutDirectVariant variant;

		TypeLayout(EmptyTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(IntegralTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(FloatTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(VariantTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(TupleTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(DynamicArrayTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(ClassTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(FunctionalTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(PointerTypeLayout layout): variant(std::move(layout)) {}

		TypeLayout(StringTypeLayout layout): variant(std::move(layout)) {}

		[[nodiscard]]
		bool operator==(const TypeLayout&) const
			= default;

		[[nodiscard]]
		const TypeLayoutDirectVariant& getVariant() const {
			return variant;
		}

		/**
		 * @copydoc TypeLayoutABC::getSize
		 */
		[[nodiscard]]
		Bits getSize() const;

		/**
		 * @copydoc TypeLayoutABC::getSourceType
		 */
		[[nodiscard]]
		tsh::AbstractType getSourceType() const;

		/**
		 * @copydoc TypeLayoutABC::toStringDefinition
		 */
		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive = true, u32 indent = 0)
			const;

		/**
		 * @copydoc TypeLayoutABC::toStringIdentification
		 */
		[[nodiscard]]
		std::string toStringIdentification() const;
	};
}
