// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "size_constants.hpp"

#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <query_framework/context/context_fd.hpp>
#include <string_id/string_id.hpp>

#include <variant>

/**
 * @brief The namespace of all definitions of the Lower Type System.
 * Short for "Type System: Low(er)".
 */
namespace compiler::tsl {
	/**
	 * @brief The abstract base class of a Type Layout object.
	 */
	class TypeLayoutABC {
		/**
		 * @brief The runtime contiguous size of a type's memory layout.
		 */
		Bits size;

		/**
		 * @brief The alignment requirement of this type, in bytes.
		 *
		 * For most types this equals the minimum of 8 and the type's byte size.
		 * For array types this equals the element alignment so that the struct
		 * offsets computed here match what LLVM produces.
		 * For struct types this equals the maximum alignment of their fields, etc.
		 */
		Bytes alignment;

		/**
		 * @brief The source type of a memory layout.
		 *
		 * @note Most layouts are created from abstract type wrapped with default values. But
		 * pointer types might be created directly from symbol type. This is important, because it
		 * allows to distinguish `T` from `ref T` from `box T` even at the memory layout level.
		 */
		tsh::SymbolType<> source_type;


		/**
		 * @brief Compact textual representation of the underlying AbstractType.
		 */
		base::StrID mangled_name;

	public:
		// Needed for default generation of copy constructor in deriving classes.
		TypeLayoutABC(const TypeLayoutABC&) = default;

		TypeLayoutABC(TypeLayoutABC&&) = delete;

		TypeLayoutABC& operator=(const TypeLayoutABC&) = delete;

		TypeLayoutABC& operator=(TypeLayoutABC&&) noexcept = delete;

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
		 * @brief Get the alignment requirement of a layout, in bytes.
		 * @return The alignment of the layout, in bytes.
		 */
		[[nodiscard]]
		Bytes getAlignment() const {
			return alignment;
		}

		/**
		 * @brief Get the source type of a layout.
		 * @return The source type of a layout.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getSourceType() const {
			return source_type;
		}

		/**
		 * @brief Get the mangled name of the source type of a layout.
		 * @return The mangled name of the source type of a layout.
		 */
		[[nodiscard]]
		base::StrID getMangledName() const {
			return mangled_name;
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
			return source_type.toString() + ":" + base::toString(getSize());
		}

		virtual ~TypeLayoutABC() = default;

	protected:
		TypeLayoutABC(Bits size, tsh::SymbolType<> source_type, query::Context& ctx);

		/**
		 * @brief Constructor with an explicit alignment override.
		 *
		 * Use this when the natural alignment of a type differs from the
		 * alignment implied by its total size (e.g. static arrays).
		 */
		TypeLayoutABC(Bits size, Bytes alignment, tsh::SymbolType<> source_type, query::Context& ctx);

		[[nodiscard]]
		static auto getIndent(const u32 indent) {
			return std::string(indent, '\t');
		}

	private:
		/**
		 * @brief Compute the default alignment for a type from its size in bits.
		 *
		 * This mirrors the alignment rules used by LLVM for scalar types:
		 * types larger than 4 bytes align to 8, larger than 2 to 4, etc.
		 */
		static Bytes computeDefaultAlignment(const Bits size_in_bits) {
			const Bytes size_in_bytes = base::bits2bytesRoundUp(size_in_bits);
			if (size_in_bytes > Bytes(4)) return Bytes(8);
			if (size_in_bytes > Bytes(2)) return Bytes(4);
			if (size_in_bytes > Bytes(1)) return Bytes(2);
			return Bytes(1);
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
		explicit EmptyTypeLayout(const tsh::UnitAbstractType unit_type, query::Context& ctx):
			  TypeLayoutABC(Bits(0), tsh::SymbolType<>::withDefaults(unit_type), ctx) {}

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "{} : " + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout for the MetaType.
	 * Represents a runtime handle/ID to type metadata.
	 *
	 * Acts as a handle that references the type information stored in the static memory.
	 *
	 * @TODO: #1709 take a look at this as well - maybe some adjustments will have to be made.
	 */
	class MetaTypeLayout final: public TypeLayoutABC {
		explicit MetaTypeLayout(const tsh::MetaAbstractType meta_type, query::Context& ctx):
			  TypeLayoutABC(META_SIZE, tsh::SymbolType<>::withDefaults(meta_type), ctx) {}

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "meta type :" + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout of a type that has integral-like low level behaviour.
	 *
	 * Valid candidates include, of course, integers, but also bytes, bools, and characters.
	 */
	class IntegralTypeLayout final: public TypeLayoutABC {
		explicit IntegralTypeLayout(const tsh::ByteAbstractType byte_type, query::Context& ctx):
			  TypeLayoutABC(BYTE_SIZE, tsh::SymbolType<>::withDefaults(byte_type), ctx) {}

		explicit IntegralTypeLayout(const tsh::BoolAbstractType bool_type, query::Context& ctx):
			  TypeLayoutABC(BOOL_SIZE, tsh::SymbolType<>::withDefaults(bool_type), ctx) {}

		explicit IntegralTypeLayout(const tsh::CharAbstractType char_type, query::Context& ctx):
			  TypeLayoutABC(CHAR_SIZE, tsh::SymbolType<>::withDefaults(char_type), ctx) {}

		explicit IntegralTypeLayout(
			const tsh::IntegralAbstractType integral_type, query::Context& ctx
		):
			  TypeLayoutABC(
				  integral_type.getSize(), tsh::SymbolType<>::withDefaults(integral_type), ctx
			  ) {}

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
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
		explicit FloatTypeLayout(const tsh::FloatAbstractType float_type, query::Context& ctx):
			  TypeLayoutABC(float_type.getSize(), tsh::SymbolType<>::withDefaults(float_type), ctx) {
		}

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "f" + std::to_string(usize(getSize())) + " : "
			     + base::toString(getSize());
		}
	};

	class StaticArrayTypeLayout final: public TypeLayoutABC {
		CRef<TypeLayout> element_layout;
		usize            element_count;

		StaticArrayTypeLayout(tsh::StaticArrayAbstractType static_array_type, query::Context& ctx);

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive, u32 indent)
			const override;

		/**
		 * @return The layout of each element of the static array.
		 */
		[[nodiscard]]
		CRef<TypeLayout> getElementLayout() const {
			return element_layout;
		}

		/**
		 * @return The number of elements in the static array.
		 */
		[[nodiscard]]
		usize getElementCount() const {
			return element_count;
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
		std::vector<CRef<TypeLayout>>       index_to_layout;

		// Delegate constructor.
		explicit VariantTypeLayout(
			const struct VariantTypeLayoutConstructionHelper& helper, query::Context& ctx
		);

		VariantTypeLayout(tsh::VariantAbstractType variant_type, query::Context& ctx);

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
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
		 * @return The type layout corresponding to that index.
		 */
		[[nodiscard]]
		CRef<TypeLayout> getLayoutOfIndex(const usize index) const {
			return index_to_layout.at(index);
		}

		/**
		 * @return The number of alternatives of this variant.
		 */
		[[nodiscard]]
		usize getNumAlternatives() const {
			return index_to_layout.size();
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
		/**
		 * @brief The number of sub-layouts in the class layout.
		 */
		usize num_sub_layouts;

		/**
		 * @brief The offsets of the fields, in bytes.
		 */
		base::Map<compiler::helios::SymID, base::Optional<Bytes>> sym_id_to_offset;

		/**
		 * @brief The layout indices of the fields.
		 */
		base::Map<compiler::helios::SymID, base::Optional<usize>> sym_id_to_layout_idx;

		/**
		 * @brief A mapping of the order of appearance in the layout to the symbol of the field.
		 */
		std::vector<compiler::helios::SymID> layout_idx_to_sym_id;

		/**
		 * @brief The field layouts, in the order of appearance in the class layout.
		 */
		std::vector<CRef<TypeLayout>> layout_idx_to_layout;

		// Delegate constructor.
		explicit ClassTypeLayout(
			struct ClassTypeLayoutConstructionHelper&& helper, query::Context& ctx
		);

		ClassTypeLayout(tsh::ClassAbstractType class_type, query::Context& ctx);

		ClassTypeLayout(tsh::TupleAbstractType tuple_type, query::Context& ctx);

		ClassTypeLayout(tsh::SliceAbstractType slice_type, query::Context& ctx);

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
		/**
		 * @brief Get the number of sub-layouts in the class layout.
		 * @return The number of sub-layouts in the class layout.
		 */
		[[nodiscard]]
		usize getNumSubLayouts() const {
			return num_sub_layouts;
		}

		/**
		 * @brief Get the offset of a field from the original type.
		 * @param symbol The symbol of a field.
		 * @return The offset of the field corresponding to the given symbol, in bytes.
		 */
		[[nodiscard]]
		base::Optional<Bytes> getOffsetOfFieldSymbol(const compiler::helios::SymID symbol) const {
			return sym_id_to_offset.at(symbol);
		}

		/**
		 * @brief Get the layout index from the field symbol in the original class type.
		 * @param symbol The symbol of a field.
		 * @return The index of the layout component corresponding to the given symbol.
		 */
		[[nodiscard]]
		base::Optional<usize> getLayoutIndexOfFieldSymbol(const compiler::helios::SymID symbol
		) const {
			return sym_id_to_layout_idx.at(symbol);
		}

		/**
		 * Get the symbol of a field from the index in which it appears in the layout.
		 * @param layout_index The index of the field in the layout order.
		 * @return The symbol of the field corresponding to the given layout index.
		 */
		[[nodiscard]]
		compiler::helios::SymID getFieldSymbolOfLayoutIndex(const usize layout_index) const {
			return layout_idx_to_sym_id.at(layout_index);
		}

		/**
		 * @brief Get the layout of a field from the index in which it appears in the layout.
		 * @param layout_index The index of the field in the layout order.
		 * @return The layout of the field corresponding to the given layout index.
		 */
		[[nodiscard]]
		CRef<TypeLayout> getFieldLayoutOfLayoutIndex(const usize layout_index) const {
			return layout_idx_to_layout.at(layout_index);
		}

		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive, u32 indent)
			const override;
	};

	/**
	 * @brief Layout of a functional object or pointer type.
	 */
	class FunctionalTypeLayout final: public TypeLayoutABC {
		// @TODO: Add support for function objects
		explicit FunctionalTypeLayout(
			const tsh::FunctionAbstractType function_type, query::Context& ctx
		):
			  TypeLayoutABC(POINTER_SIZE, tsh::SymbolType<>::withDefaults(function_type), ctx) {}

		friend struct ImplementationOf_QueryAbstractTypeLayout;

	public:
		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			return getIndent(indent) + "Functional : " + base::toString(getSize());
		}
	};

	/**
	 * @brief Layout of a type that has pointer-like low level behaviour.
	 */
	class PointerTypeLayout final: public TypeLayoutABC {
	public:
		enum class PointerKind { SinglePointer, ManyPointer, CPointer };

	private:
		// The type of the pointee. Its layout is looked up lazily (see getPointee), so that a
		// type may hold a pointer to itself (e.g. `class T { t: ptr T; }`) without a query
		// cycle. Since a pointer may be untyped, the pointee may be unknown, hence the Optional.
		base::Optional<tsh::SymbolType<>> pointee_type{};

		PointerKind pointer_kind;

		/**
		 * @brief Construct a PointerLayout for a RawPointer.
		 * @param raw_pointer_type The source RawPointer.
		 */
		explicit PointerTypeLayout(
			const tsh::RawPointerAbstractType raw_pointer_type, query::Context& ctx
		):
			  TypeLayoutABC(POINTER_SIZE, tsh::SymbolType<>::withDefaults(raw_pointer_type), ctx),
			  pointer_kind(PointerKind::SinglePointer) {}

		/**
		 * @brief Construct a PointerLayout from a typed Pointer.
		 * @param pointer_type The source typed Pointer.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::PointerAbstractType pointer_type, query::Context& ctx);

		/**
		 * @brief Construct a PointerLayout from a typed ManyPointer.
		 * @param pointer_type The source typed ManyPointer.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::ManyPointerAbstractType pointer_type, query::Context& ctx);

		/**
		 * @brief Construct a PointerLayout from a typed CPointer.
		 * @param pointer_type The source typed CPointer.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::CPointerAbstractType pointer_type, query::Context& ctx);

		/**
		 * @brief Construct a PointerLayout from a SymbolType, provided that it is not DIRECT.
		 * @param symbol_type A type with reference indirection, i.e. not DIRECT reference kind.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerTypeLayout(tsh::SymbolType<> symbol_type, query::Context& ctx);

		friend struct ImplementationOf_QueryAbstractTypeLayout;
		friend struct ImplementationOf_QuerySymbolTypeLayout;

	public:
		/**
		 * @brief Get the type of the pointee, if the pointer is typed.
		 * @return The pointee type, or an empty Optional for an untyped pointer.
		 */
		[[nodiscard]]
		base::Optional<tsh::SymbolType<>> getPointeeTypeOpt() const {
			return pointee_type;
		}

		/**
		 * @brief Get the layout of the pointee. The pointer must be typed.
		 * @param ctx The query::Context used to look up the layout of the pointee.
		 * @return The layout of the pointee type.
		 */
		[[nodiscard]]
		CRef<TypeLayout> getPointee(query::Context& ctx) const;

		/**
		 * @brief Check whether the pointer is typed, i.e. whether it has a pointee.
		 * @return Whether the pointer has a pointee.
		 */
		[[nodiscard]]
		bool hasPointee() const {
			return pointee_type.has_value();
		}

		[[nodiscard]] PointerKind getPointerKind() const { return pointer_kind; }

		[[nodiscard]]
		std::string toStringDefinition(query::Context&, bool, const u32 indent) const override {
			std::string pointer_kind_str;
			switch (pointer_kind) {
			case PointerKind::SinglePointer:
				pointer_kind_str = "Pointer";
				break;
			case PointerKind::ManyPointer:
				pointer_kind_str = "ManyPointer";
				break;
			case PointerKind::CPointer:
				pointer_kind_str = "CPointer";
				break;
			}
			return getIndent(indent) + pointer_kind_str + " to " + getSourceType().toString()
			     + " : " + base::toString(getSize());
		}
	};

	using TypeLayoutDirectVariant = std::variant<
		EmptyTypeLayout,
		MetaTypeLayout,
		IntegralTypeLayout,
		FloatTypeLayout,
		VariantTypeLayout,
		StaticArrayTypeLayout,
		ClassTypeLayout,
		FunctionalTypeLayout,
		PointerTypeLayout>;

	/**
	 * @brief The ADT representing the layout of a type.
	 *
	 * @note Use getVariant() when matching against the variant's options.
	 */
	class TypeLayout final {
		// Copy constructor needed for QuerySymbolTypeLayout.
		TypeLayout(const TypeLayout& other) = default;

		friend struct ImplementationOf_QuerySymbolTypeLayout;

		TypeLayoutDirectVariant variant;

	public:
		// Move constructor needed for caching in QueryAbstract/SymbolTypeLayout.
		TypeLayout(TypeLayout&& other) noexcept = default;

		// Copy and move assignments are deleted to avoid accidental copies.
		TypeLayout& operator=(const TypeLayout& other)     = delete;
		TypeLayout& operator=(TypeLayout&& other) noexcept = delete;

		// Constructor for any variant option.
		template<typename T>
		TypeLayout(T&& layout): variant(std::forward<T>(layout)) {}

		[[nodiscard]]
		bool operator==(const TypeLayout&) const
			= default;

		[[nodiscard]]
		const TypeLayoutDirectVariant& getVariant() const {
			return variant;
		}

		template<typename T>
		[[nodiscard]]
		bool is() const {
			return std::holds_alternative<T>(variant);
		}

		template<typename T>
		[[nodiscard]]
		const T& as() const {
			CORE_ASSERT(is<T>(), "Invalid type layout variant access");
			return std::get<T>(variant);
		}

		/**
		 * @brief Get the total size of a layout, in bits.
		 * @return The total size of a layout, in bits.
		 */
		[[nodiscard]]
		Bits getSize() const;

		/**
		 * @brief Get the alignment requirement of a layout, in bytes.
		 * @return The alignment of the layout, in bytes.
		 */
		[[nodiscard]]
		Bytes getAlignment() const;

		/**
		 * @brief Get the source type of a layout.
		 * @return The source type of a layout.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getSourceType() const;

		/**
		 * @brief Get a string describing the layout in a human-friendly format.
		 * @param ctx The query context for fetching layouts of components in composite layouts.
		 * @param recursive Whether the layout string should contain full layout strings of
		 * composite component types. Setting this to `false` will result in the use of IDs instead.
		 * @param indent The indent at which to print. Mostly used internally for recursive prints.
		 * @return A string describing the layout.
		 */
		[[nodiscard]]
		std::string toStringDefinition(query::Context& ctx, bool recursive = true, u32 indent = 0)
			const;

		/**
		 * @brief Get a relatively short string identifying the type layout.
		 * @return A string identifying the type layout.
		 */
		[[nodiscard]]
		std::string toStringIdentification() const;

		/**
		 * @brief Get the mangled name of the source type of a layout.
		 * @return The mangled name of the source type of a layout.
		 */
		[[nodiscard]]
		base::StrID getMangledName() const;
	};
}
