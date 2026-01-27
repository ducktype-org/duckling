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

#include <query_framework/internal/query_metadata/metadata_storage.hpp>

#include <concepts>
#include <cstddef>
#include <cstring>
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
	 */
	template<typename T>
	concept TriviallySerializable = std::is_trivially_copyable_v<T>;

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
 * // Creates: struct metadata_SourceLocation : public query::BaseMetadata { ... }
 * @endcode
 */
#define DECLARE_METADATA(name, typ)                                                            \
	struct metadata_##name final: public ::query::BaseMetadata {                               \
		static_assert(                                                                         \
			::query::HasSerialize<typ>,                                                        \
			"Type '" #typ "' must implement 'std::vector<std::byte> serialize() const'"        \
		);                                                                                     \
		static_assert(                                                                         \
			::query::HasDeserialize<typ>,                                                      \
			"Type '" #typ "' must implement 'static " #typ                                     \
			" deserialize(std::span<const std::byte>)'"                                        \
		);                                                                                     \
                                                                                               \
		static inline const base::StrID TYPE_ID{ std::string{ "metadata_" #name } };           \
                                                                                               \
		typ value;                                                                             \
                                                                                               \
		metadata_##name() = delete;                                                            \
                                                                                               \
		explicit metadata_##name(typ val): value(std::move(val)) {}                            \
                                                                                               \
		template<typename... Args>                                                             \
		requires std::constructible_from<typ, Args...>                                         \
		explicit metadata_##name(Args&&... args): value(std::forward<Args>(args)...) {}        \
                                                                                               \
		[[nodiscard]]                                                                          \
		std::vector<std::byte> serialize() const override {                                    \
			return value.serialize();                                                          \
		}                                                                                      \
                                                                                               \
		[[nodiscard]]                                                                          \
		base::StrID getTypeID() const override {                                               \
			return TYPE_ID;                                                                    \
		}                                                                                      \
                                                                                               \
		[[nodiscard]]                                                                          \
		static metadata_##name deserialize(std::span<const std::byte> data) {                  \
			return metadata_##name{ typ::deserialize(data) };                                  \
		}                                                                                      \
                                                                                               \
	private:                                                                                   \
		static Box<::query::BaseMetadata> _deserialize(std::span<const std::byte> data) {      \
			return makeBox<metadata_##name>(metadata_##name::deserialize(data));               \
		}                                                                                      \
		static bool _doRegister() {                                                            \
			return ::query::MetadataRegistry::instance().registerType(TYPE_ID, &_deserialize); \
		}                                                                                      \
		static inline bool _registered = _doRegister();                                        \
	}

/**
 * @brief Macro to declare a metadata type for trivially copyable values.
 *
 * Creates a struct `metadata_##name` that wraps a value of type `typ`.
 * The wrapped type must be trivially copyable (std::is_trivially_copyable_v<typ> == true).
 *
 * Serialization/deserialization is automatic using memcpy.
 *
 * @param name The name suffix for the metadata struct (creates metadata_##name)
 * @param typ The type of value this metadata wraps (must be trivially copyable)
 *
 * Example:
 * @code
 * DECLARE_METADATA_SIMPLE(Counter, u64);
 * // Creates: struct metadata_Counter : public query::BaseMetadata { u64 value; ... }
 * @endcode
 */
#define DECLARE_METADATA_SIMPLE(name, typ)                                                     \
	struct metadata_##name final: public ::query::BaseMetadata {                               \
		static_assert(                                                                         \
			::query::TriviallySerializable<typ>,                                               \
			"Type '" #typ                                                                      \
			"' must be trivially copyable for DECLARE_METADATA_SIMPLE. "                       \
			"Use DECLARE_METADATA for complex types."                                          \
		);                                                                                     \
                                                                                               \
		static inline const base::StrID TYPE_ID{ std::string{ "metadata_" #name } };           \
                                                                                               \
		typ value;                                                                             \
                                                                                               \
		metadata_##name() = delete;                                                            \
                                                                                               \
		explicit metadata_##name(typ val): value(std::move(val)) {}                            \
                                                                                               \
		[[nodiscard]]                                                                          \
		std::vector<std::byte> serialize() const override {                                    \
			std::vector<std::byte> result(sizeof(typ));                                        \
			std::memcpy(result.data(), &value, sizeof(typ));                                   \
			return result;                                                                     \
		}                                                                                      \
                                                                                               \
		[[nodiscard]]                                                                          \
		base::StrID getTypeID() const override {                                               \
			return TYPE_ID;                                                                    \
		}                                                                                      \
                                                                                               \
		[[nodiscard]]                                                                          \
		static metadata_##name deserialize(std::span<const std::byte> data) {                  \
			CORE_ASSERT(data.size() >= sizeof(typ), "Insufficient data for deserialization");  \
			alignas(typ) std::array<unsigned char, sizeof(typ)> buffer{};                      \
			std::memcpy(buffer.data(), data.data(), sizeof(typ));                              \
			return metadata_##name{ *reinterpret_cast<const typ*>(buffer.data()) };            \
		}                                                                                      \
                                                                                               \
	private:                                                                                   \
		static Box<::query::BaseMetadata> _deserialize(std::span<const std::byte> data) {      \
			return makeBox<metadata_##name>(metadata_##name::deserialize(data));               \
		}                                                                                      \
		static bool _doRegister() {                                                            \
			return ::query::MetadataRegistry::instance().registerType(TYPE_ID, &_deserialize); \
		}                                                                                      \
		static inline bool _registered = _doRegister();                                        \
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
 * // Creates: struct metadata_SourceFile : public query::BaseMetadata { base::StrID value; ... }
 *
 * // Usage:
 * ctx.addMetadata<metadata_SourceFile>(base::StrID{"src/main.duck"});
 * @endcode
 */
#define DECLARE_METADATA_STRID(name)                                                              \
	struct metadata_##name final: public ::query::BaseMetadata {                                  \
		static inline const base::StrID TYPE_ID{ std::string{ "metadata_" #name } };              \
                                                                                                  \
		base::StrID value;                                                                        \
                                                                                                  \
		metadata_##name() = delete;                                                               \
                                                                                                  \
		explicit metadata_##name(base::StrID val): value(std::move(val)) {}                       \
                                                                                                  \
		[[nodiscard]]                                                                             \
		std::vector<std::byte> serialize() const override {                                       \
			/* Not used for StrID types - serialization uses getStrIDValue() */                   \
			return {};                                                                            \
		}                                                                                         \
                                                                                                  \
		[[nodiscard]]                                                                             \
		base::StrID getTypeID() const override {                                                  \
			return TYPE_ID;                                                                       \
		}                                                                                         \
                                                                                                  \
		[[nodiscard]]                                                                             \
		bool usesStrIDTable() const override {                                                    \
			return true;                                                                          \
		}                                                                                         \
                                                                                                  \
		[[nodiscard]]                                                                             \
		base::StrID getStrIDValue() const override {                                              \
			return value;                                                                         \
		}                                                                                         \
                                                                                                  \
	private:                                                                                      \
		static Box<::query::BaseMetadata> _fromStrID(base::StrID str_value) {                     \
			return makeBox<metadata_##name>(str_value);                                           \
		}                                                                                         \
		static bool _doRegister() {                                                               \
			return ::query::MetadataRegistry::instance().registerStrIDType(TYPE_ID, &_fromStrID); \
		}                                                                                         \
		static inline bool _registered = _doRegister();                                           \
	}
