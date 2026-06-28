/**
 * @file declare_metadata.hpp
 * @brief DECLARE_METADATA macro for easy metadata type declaration.
 *
 * This file provides macros for declaring metadata types:
 * - DECLARE_METADATA: for types with custom serialize/deserialize methods
 * - DECLARE_METADATA_SIMPLE: for trivially copyable types (automatic serialization)
 * - DECLARE_METADATA_STRID: for StrID values (optimized string table serialization)
 *
 * Usage:
 * @code
 * // For types with custom serialization:
 * DECLARE_METADATA(MyMeta, MySerializableType);
 *
 * // For trivially copyable types (e.g., u64, structs of primitives):
 * DECLARE_METADATA_SIMPLE(Counter, u64);
 *
 * // For StrID values (optimized for repeated strings like file names):
 * DECLARE_METADATA_STRID(SourceFile);
 *
 * // Use in query provide():
 * ctx.addMetadata<metadata_MyMeta>(my_value);
 * ctx.addMetadata<metadata_SourceFile>(base::StrID{"src/main.duck"});
 *
 * // Retrieve metadata:
 * auto all = state.getMetadata<metadata_MyMeta>(node_id);
 * @endcode
 */
#pragma once

#include <query_framework/internal/query_metadata/metadata_registry.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>

#include <concepts>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <span>
#include <type_traits>
#include <vector>

namespace query {

	/**
	 * @brief Concept checking if a type has a serialize() method returning std::vector<std::byte>.
	 */
	template<typename T>
	concept HasSerialize = requires(const T& t) {
		{ t.serialize() } -> std::same_as<std::vector<std::byte>>;
	};

	/**
	 * @brief Concept checking if a type has a static deserialize() method.
	 */
	template<typename T>
	concept HasDeserialize = requires(std::span<const std::byte> data) {
		{ T::deserialize(data) } -> std::same_as<T>;
	};

	/**
	 * @brief Concept for types that support full serialization.
	 */
	template<typename T>
	concept Serializable = HasSerialize<T> && HasDeserialize<T>;

	/**
	 * @brief Concept for trivially copyable types that can be serialized automatically.
	 * More info: https://en.cppreference.com/w/cpp/types/is_trivially_copyable.html
	 */
	template<typename T>
	concept TriviallySerializable = std::is_trivially_copyable_v<
		T>;  // && std::is_implicit_lifetime_v<T>: This is not supported by gcc14, although
	         // std::is_trivially_copyable_v<T> implies std::is_implicit_lifetime_v<T>

	/**
	 * @brief Concept checking if a type has a 'void prettyPrint(std::ostream&) const' method.
	 *
	 * When the wrapped type satisfies this concept, the generated metadata struct overrides
	 * BaseMetadata::prettyPrint to forward to the wrapped value's prettyPrint.
	 */
	template<typename T>
	concept HasPrettyPrint = requires(const T& t, std::ostream& os) {
		{ t.prettyPrint(os) } -> std::same_as<void>;
	};

	/**
	 * @brief Pretty print a wrapped metadata value, forwarding to its own prettyPrint if available.
	 *
	 * If the value type defines 'void prettyPrint(std::ostream&) const' the call is forwarded to it.
	 * Otherwise the default BaseMetadata::prettyPrint implementation is used.
	 *
	 * This is a function template so that the unused branch is discarded by 'if constexpr'; the same
	 * dispatch written inline in a non-templated override would require both branches to be
	 * well-formed for the concrete value type.
	 *
	 * @param os The output stream to print to.
	 * @param value The wrapped value to print.
	 * @param base The owning metadata instance, used for the default fallback.
	 */
	template<typename T>
	void prettyPrintMetadataValue(
		std::ostream& os, const T& value, const internal::BaseMetadata& base
	) {
		if constexpr (HasPrettyPrint<T>) {
			value.prettyPrint(os);
		} else {
			base.internal::BaseMetadata::prettyPrint(os);
		}
	}

}  // namespace query

/**
 * @brief Macro to declare a serializable metadata type.
 *
 * Creates a struct `metadata_##name` that wraps a value of type `typ`.
 * The wrapped type must satisfy the Serializable concept (have serialize/deserialize methods).
 *
 * The macro also automatically registers the type with MetadataRegistry for deserialization.
 *
 * @param name The name suffix for the metadata struct (creates metadata_##name)
 * @param typ The type of value this metadata wraps
 *
 * Example:
 * @code
 * DECLARE_METADATA(SourceLocation, SourceLocationData);
 * // Usage:
 * ctx.addMetadata<metadata_SourceLocation>(loc_data);
 * @endcode
 */
#define DECLARE_METADATA(name, type) INTERNAL_DECLARE_METADATA(metadata_##name, type)

#define INTERNAL_DECLARE_METADATA(Metadata, type)                                                  \
	struct Metadata final: public ::query::internal::BaseMetadata {                                \
		static_assert(                                                                             \
			::query::HasSerialize<type>,                                                           \
			"type '" #type "' must implement 'std::vector<std::byte> serialize() const'"           \
		);                                                                                         \
		static_assert(                                                                             \
			::query::HasDeserialize<type>,                                                         \
			"Type '" #type "' must implement 'static " #type                                       \
			" deserialize(std::span<const std::byte>)'"                                            \
		);                                                                                         \
                                                                                                   \
		static inline const base::StrID TYPE_ID{ std::string{ #Metadata } };                       \
                                                                                                   \
		type value;                                                                                \
                                                                                                   \
		Metadata() = delete;                                                                       \
                                                                                                   \
                                                                                                   \
		template<typename... Args>                                                                 \
		requires std::constructible_from<type, Args...>                                            \
		explicit Metadata(Args&&... args): value(std::forward<Args>(args)...) {}                   \
                                                                                                   \
		[[nodiscard]]                                                                              \
		std::vector<std::byte> serialize() const override {                                        \
			return value.serialize();                                                              \
		}                                                                                          \
                                                                                                   \
		[[nodiscard]]                                                                              \
		base::StrID getTypeID() const override {                                                   \
			return TYPE_ID;                                                                        \
		}                                                                                          \
                                                                                                   \
		void prettyPrint(std::ostream& os) const override {                                        \
			::query::prettyPrintMetadataValue(os, value, *this);                                   \
		}                                                                                          \
                                                                                                   \
		[[nodiscard]]                                                                              \
		static Metadata deserialize(std::span<const std::byte> data) {                             \
			return Metadata{ type::deserialize(data) };                                            \
		}                                                                                          \
                                                                                                   \
	private:                                                                                       \
		static Box<::query::internal::BaseMetadata> boxDeserialize(std::span<const std::byte> data \
		) {                                                                                        \
			return makeBox<Metadata>(Metadata::deserialize(data));                                 \
		}                                                                                          \
		static bool doRegister() {                                                                 \
			return ::query::internal::MetadataRegistry::instance().registerType(                   \
				TYPE_ID, &boxDeserialize                                                           \
			);                                                                                     \
		}                                                                                          \
		static inline bool registered = doRegister();                                              \
	}

/**
 * @brief Macro to declare a metadata type for trivially copyable values.
 *
 * Creates a struct `metadata_##name` that wraps a value of type `type`.
 * The wrapped type must be trivially copyable (std::is_trivially_copyable_v<type> == true).
 *
 * Serialization/deserialization is automatic using memcpy.
 *
 * @param name The name suffix for the metadata struct (creates metadata_##name)
 * @param type The type of value this metadata wraps (must be trivially copyable)
 *
 * Example:
 * @code
 * DECLARE_METADATA_SIMPLE(Counter, u64);
 * // Usage:
 * ctx.addMetadata<metadata_Counter>(42);
 * @endcode
 */
#define DECLARE_METADATA_SIMPLE(name, type) INTERNAL_DECLARE_METADATA_SIMPLE(metadata_##name, type)

#define INTERNAL_DECLARE_METADATA_SIMPLE(Metadata, type)                                           \
	struct Metadata final: public ::query::internal::BaseMetadata {                                \
		static_assert(                                                                             \
			::query::TriviallySerializable<type>,                                                  \
			"Type '" #type                                                                         \
			"' must be trivially copyable for DECLARE_METADATA_SIMPLE. "                           \
			"Use DECLARE_METADATA for complex types."                                              \
		);                                                                                         \
                                                                                                   \
		static inline const base::StrID TYPE_ID{ std::string{ #Metadata } };                       \
                                                                                                   \
		type value;                                                                                \
                                                                                                   \
		Metadata() = delete;                                                                       \
                                                                                                   \
		explicit Metadata(type val): value(std::move(val)) {}                                      \
                                                                                                   \
		[[nodiscard]]                                                                              \
		std::vector<std::byte> serialize() const override {                                        \
			std::vector<std::byte> result(sizeof(type));                                           \
			std::memcpy(result.data(), &value, sizeof(type));                                      \
			return result;                                                                         \
		}                                                                                          \
                                                                                                   \
		[[nodiscard]]                                                                              \
		base::StrID getTypeID() const override {                                                   \
			return TYPE_ID;                                                                        \
		}                                                                                          \
                                                                                                   \
		void prettyPrint(std::ostream& os) const override {                                        \
			::query::prettyPrintMetadataValue(os, value, *this);                                   \
		}                                                                                          \
                                                                                                   \
		[[nodiscard]]                                                                              \
		static Metadata deserialize(std::span<const std::byte> data) {                             \
			CORE_ASSERT(data.size() == sizeof(type), "Bad data size for deserialization");         \
			alignas(type) std::array<unsigned char, sizeof(type)> buffer{};                        \
			std::memcpy(buffer.data(), data.data(), sizeof(type));                                 \
			return Metadata{ *std::launder(reinterpret_cast<const type*>(buffer.data())) };        \
		}                                                                                          \
                                                                                                   \
	private:                                                                                       \
		static Box<::query::internal::BaseMetadata> boxDeserialize(std::span<const std::byte> data \
		) {                                                                                        \
			return makeBox<Metadata>(Metadata::deserialize(data));                                 \
		}                                                                                          \
		static bool doRegister() {                                                                 \
			return ::query::internal::MetadataRegistry::instance().registerType(                   \
				TYPE_ID, &boxDeserialize                                                           \
			);                                                                                     \
		}                                                                                          \
		static inline bool registered = doRegister();                                              \
	}

/**
 * @brief Macro to declare a metadata type for StrID values with optimized serialization.
 *
 * Creates a struct `metadata_##name` that wraps a base::StrID value.
 * During serialization, StrID values are collected into a global string table
 * and only the table index is stored, saving space for repeated strings.
 *
 * @param name The name suffix for the metadata struct (creates metadata_##name)
 *
 * Example:
 * @code
 * DECLARE_METADATA_STRID(SourceFile);
 * // Usage:
 * ctx.addMetadata<metadata_SourceFile>(base::StrID{"src/main.duck"});
 * @endcode
 */
#define DECLARE_METADATA_STRID(name) INTERNAL_DECLARE_METADATA_STRID(metadata_##name)

#define INTERNAL_DECLARE_METADATA_STRID(Metadata)                                      \
	struct Metadata final: public ::query::internal::BaseMetadata {                    \
		static inline const base::StrID TYPE_ID{ std::string{ #Metadata } };           \
                                                                                       \
		base::StrID value;                                                             \
                                                                                       \
		Metadata() = delete;                                                           \
                                                                                       \
		explicit Metadata(base::StrID val): value(std::move(val)) {}                   \
                                                                                       \
		[[nodiscard]]                                                                  \
		std::vector<std::byte> serialize() const override {                            \
			CORE_PANIC("StrID metadata should use string table serialization");        \
			CORE_UNREACHABLE();                                                        \
		}                                                                              \
                                                                                       \
		[[nodiscard]]                                                                  \
		base::StrID getTypeID() const override {                                       \
			return TYPE_ID;                                                            \
		}                                                                              \
                                                                                       \
		[[nodiscard]]                                                                  \
		bool usesStrIDTable() const override {                                         \
			return true;                                                               \
		}                                                                              \
                                                                                       \
		[[nodiscard]]                                                                  \
		base::StrID getStrIDValue() const override {                                   \
			return value;                                                              \
		}                                                                              \
                                                                                       \
		void prettyPrint(std::ostream& os) const override {                            \
			::query::prettyPrintMetadataValue(os, value, *this);                       \
		}                                                                              \
                                                                                       \
	private:                                                                           \
		static Box<::query::internal::BaseMetadata> fromStrID(base::StrID str_value) { \
			return makeBox<Metadata>(str_value);                                       \
		}                                                                              \
		static bool doRegister() {                                                     \
			return ::query::internal::MetadataRegistry::instance().registerStrIDType(  \
				TYPE_ID, &fromStrID                                                    \
			);                                                                         \
		}                                                                              \
		static inline bool registered = doRegister();                                  \
	}
