// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "metadata_storage.hpp"

#include <variant>

namespace query::internal {
	/**
	 * @brief Function pointer type for reading metadata straight out of the metadata stream.
	 * @note The archive is pinned to MetadataIn for the same reason BaseMetadata::serWrite
	 * pins its own: this is a function POINTER, and a template has no address.
	 */
	using ArchiveDeserializeFunc = Box<BaseMetadata> (*)(MetadataIn&);

	/**
	 * @brief Function pointer type for deserializing StrID metadata from string table.
	 */
	using StrIDDeserializeFunc = Box<BaseMetadata> (*)(base::StrID);

	/**
	 * @brief Variant holding either bytes or StrID deserializer.
	 *
	 * This ensures each type is either a normal type (read from the stream)
	 * or a StrID type (deserialized from string table index), but never both.
	 */
	using DeserializerVariant = std::variant<ArchiveDeserializeFunc, StrIDDeserializeFunc>;

	/**
	 * @brief Global registry for metadata types.
	 *
	 * This registry maps type IDs to their deserialize functions,
	 * enabling runtime deserialization of metadata from serialized data.
	 *
	 * Each type is either:
	 * - Normal type: read from the metadata stream (ArchiveDeserializeFunc)
	 * - StrID type: deserialized from string table (StrIDDeserializeFunc)
	 *
	 * Types are automatically registered when DECLARE_METADATA macro is used.
	 */
	class MetadataRegistry final {
	public:
		/**
		 * @brief Data structure for registered metadata type.
		 */
		struct RegisterData final {
			DeserializerVariant deserializer;
		};

		using TypeID = BaseMetadata::TypeID;

	private:
		base::StableHashMap<TypeID, RegisterData> registry;

		MetadataRegistry() = default;

	public:
		MetadataRegistry(const MetadataRegistry&)            = delete;
		MetadataRegistry(MetadataRegistry&&)                 = delete;
		MetadataRegistry& operator=(const MetadataRegistry&) = delete;
		MetadataRegistry& operator=(MetadataRegistry&&)      = delete;

		/**
		 * @brief Get the singleton instance of the registry.
		 */
		static MetadataRegistry& instance();

		/**
		 * @brief Register a metadata type with an archive deserializer.
		 *
		 * @param type_id The unique StrID of the metadata type.
		 * @param deserialize_func Function pointer to read the metadata out of the stream.
		 * @return true (always succeeds, asserts on duplicate registration).
		 */
		bool registerType(TypeID type_id, ArchiveDeserializeFunc deserialize_func);

		/**
		 * @brief Register a StrID metadata type with StrID deserializer.
		 *
		 * @param type_id The unique StrID of the metadata type.
		 * @param deserialize_func Function pointer to deserialize the metadata from StrID.
		 * @return true (always succeeds, asserts on duplicate registration).
		 */
		bool registerStrIDType(TypeID type_id, StrIDDeserializeFunc deserialize_func);

		/**
		 * @brief Get the deserializer variant for a type.
		 *
		 * @param type_id The type ID of the metadata.
		 * @return Optional containing the deserializer variant, or empty if not found.
		 */
		[[nodiscard]]
		DeserializerVariant getDeserializer(TypeID type_id) const;

		/**
		 * @brief Check if a type is registered.
		 */
		[[nodiscard]]
		bool isRegistered(TypeID type_id) const;

		/**
		 * @brief Check if a type is registered as StrID type.
		 */
		[[nodiscard]]
		bool isStrIDType(TypeID type_id) const;
	};
}
