/**
 * @file declare_metadata.hpp
 * @brief DECLARE_METADATA macro for easy metadata type declaration.
 *
 * This file provides macros for declaring metadata types:
 * - DECLARE_METADATA: for any type the `ser` module can put on the wire, from a u64 up
 * - DECLARE_METADATA_STRID: for StrID values (optimized string table serialization)
 *
 * The bytes come from the `ser` module, so the wrapped type needs no serialization code of
 * its own: an aggregate is walked field by field, and a type that needs more says so with
 * one of the `ser` hooks (`serVisit`, `serWrite` + `serRead`, `serWrite` + `serMake`) or a
 * `ser::Serializer<T>` specialization. See @ref dev/src/common/ser/readme.md.
 *
 * Usage:
 * @code
 * // For any serializable type - an aggregate needs nothing at all:
 * DECLARE_METADATA(MyMeta, MySerializableType);
 *
 * // A scalar is not a special case either:
 * DECLARE_METADATA(Counter, u64);
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
#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

#include <concepts>
#include <cstddef>
#include <ostream>
#include <span>
#include <vector>

namespace query {

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
	 * If the value type defines 'void prettyPrint(std::ostream&) const' the call is forwarded to
	 * it. Otherwise the default BaseMetadata::prettyPrint implementation is used.
	 *
	 * This is a function template so that the unused branch is discarded by 'if constexpr'; the
	 * same dispatch written inline in a non-templated override would require both branches to be
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
		if constexpr (HasPrettyPrint<T>)
			value.prettyPrint(os);
		else
			base.internal::BaseMetadata::prettyPrint(os);
	}

}  // namespace query

/**
 * @brief Macro to declare a serializable metadata type.
 *
 * Creates a struct `metadata_##name` that wraps a value of type `typ`.
 * The value is written into - and read straight back out of - the metadata stream by the
 * `ser` archive the whole storage uses, so the wrapped type needs no serialization code of
 * its own unless its format is not just its fields; see the note at the top of this file.
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

#define INTERNAL_DECLARE_METADATA(Metadata, type)                                                   \
	static_assert(                                                                                  \
		::ser::MIN_WIRE_SIZE_V<type> > 0,                                                           \
		"query: this metadata type writes NO bytes, and MetadataStorage relies on "                 \
		"every entry reading at least one byte"                                                     \
	);                                                                                              \
	struct Metadata final: public ::query::internal::BaseMetadata {                                 \
		static inline const base::StrID TYPE_ID{ std::string{ #Metadata } };                        \
                                                                                                    \
		type value;                                                                                 \
                                                                                                    \
		Metadata() = delete;                                                                        \
                                                                                                    \
                                                                                                    \
		template<typename... Args>                                                                  \
		requires std::constructible_from<type, Args...>                                             \
		explicit Metadata(Args&&... args): value(std::forward<Args>(args)...) {}                    \
                                                                                                    \
		[[nodiscard]]                                                                               \
		::ser::Errc serWrite(::query::internal::MetadataOut& ar) const override {                   \
			return ar(value);                                                                       \
		}                                                                                           \
                                                                                                    \
		[[nodiscard]]                                                                               \
		base::StrID getTypeID() const override {                                                    \
			return TYPE_ID;                                                                         \
		}                                                                                           \
                                                                                                    \
		void prettyPrint(std::ostream& os) const override {                                         \
			::query::prettyPrintMetadataValue(os, value, *this);                                    \
		}                                                                                           \
                                                                                                    \
		[[nodiscard]]                                                                               \
		static Metadata serMake(::query::internal::MetadataIn& ar) {                                \
			return Metadata{ ::ser::subMake<type>(ar) };                                            \
		}                                                                                           \
                                                                                                    \
	private:                                                                                        \
		static Box<::query::internal::BaseMetadata> boxSerMake(::query::internal::MetadataIn& ar) { \
			return makeBox<Metadata>(::ser::subMake<type>(ar));                                     \
		}                                                                                           \
		static bool doRegister() {                                                                  \
			return ::query::internal::MetadataRegistry::instance().registerType(                    \
				TYPE_ID, &boxSerMake                                                                \
			);                                                                                      \
		}                                                                                           \
		static inline bool registered = doRegister();                                               \
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
		::ser::Errc serWrite(::query::internal::MetadataOut&) const override {         \
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
