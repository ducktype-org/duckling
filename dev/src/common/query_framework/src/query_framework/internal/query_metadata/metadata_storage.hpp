/**
 * @file metadata_storage.hpp
 * @brief Internal metadata storage infrastructure for Query Framework.
 *
 * This file provides the core infrastructure for the metadata system:
 * - BaseMetadata: abstract base class for all metadata types
 * - MetadataRegistry: type registry for serialization/deserialization
 * - MetadataStorage: storage container for metadata attached to NodeIDs
 *
 * @note Metadata can only be added to queries with preserve_in_graph = true
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>
#include <string_id/string_id.hpp>

#include <cstddef>
#include <cstring>
#include <span>
#include <variant>
#include <vector>

namespace query {

	/**
	 * @brief Abstract base class for all metadata types.
	 *
	 * All metadata types must inherit from this class and implement
	 * the serialize() method. The deserialize() method should be
	 * implemented as a static method in the derived class.
	 *
	 * @note Use the DECLARE_METADATA macro to create new metadata types,
	 * which handles the boilerplate automatically.
	 */
	struct BaseMetadata {
		virtual ~BaseMetadata() = default;

		/**
		 * @brief Serialize the metadata to a byte vector.
		 * @return std::vector<std::byte> The serialized data.
		 */
		[[nodiscard]]
		virtual std::vector<std::byte> serialize() const
			= 0;

		/**
		 * @brief Get the type ID (StrID) of this metadata type.
		 * Used for serialization and runtime type identification.
		 * @return base::StrID The unique type identifier.
		 */
		[[nodiscard]]
		virtual base::StrID getTypeID() const
			= 0;

		/**
		 * @brief Check if this metadata type uses StrID table for optimized serialization.
		 *
		 * When true, the metadata value is a StrID that will be serialized as an index
		 * into a global string table, saving space for repeated strings.
		 *
		 * @return true if this type uses the StrID table optimization
		 */
		[[nodiscard]]
		virtual bool usesStrIDTable() const {
			return false;
		}

		/**
		 * @brief Get the StrID value for table-based serialization.
		 *
		 * Only valid when usesStrIDTable() returns true.
		 * Used during serialization to collect all StrIDs for the string table.
		 *
		 * @return The StrID value of this metadata
		 */
		[[nodiscard]]
		virtual base::StrID getStrIDValue() const {
			return base::StrID{};  // Default: empty
		}
	};

	/**
	 * @brief Function pointer type for deserializing metadata from bytes.
	 */
	using BytesDeserializeFunc = Box<BaseMetadata> (*)(std::span<const std::byte>);

	/**
	 * @brief Function pointer type for deserializing StrID metadata from string table.
	 */
	using StrIDDeserializeFunc = Box<BaseMetadata> (*)(base::StrID);

	/**
	 * @brief Variant holding either bytes or StrID deserializer.
	 *
	 * This ensures each type is either a normal type (deserialized from bytes)
	 * or a StrID type (deserialized from string table index), but never both.
	 */
	using DeserializerVariant = std::variant<BytesDeserializeFunc, StrIDDeserializeFunc>;

	/**
	 * @brief Global registry for metadata types.
	 *
	 * This registry maps type IDs to their deserialize functions,
	 * enabling runtime deserialization of metadata from serialized data.
	 *
	 * Each type is either:
	 * - Normal type: deserialized from bytes (BytesDeserializeFunc)
	 * - StrID type: deserialized from string table (StrIDDeserializeFunc)
	 *
	 * Types are automatically registered when DECLARE_METADATA macro is used.
	 */
	class MetadataRegistry final {
	private:
		base::HashMap<base::StrID, DeserializerVariant> registry_;

		MetadataRegistry() = default;

	public:
		MetadataRegistry(const MetadataRegistry&)            = delete;
		MetadataRegistry(MetadataRegistry&&)                 = delete;
		MetadataRegistry& operator=(const MetadataRegistry&) = delete;
		MetadataRegistry& operator=(MetadataRegistry&&)      = delete;

		/**
		 * @brief Get the singleton instance of the registry.
		 */
		static MetadataRegistry& instance() {
			static MetadataRegistry inst;
			return inst;
		}

		/**
		 * @brief Register a metadata type with bytes deserializer.
		 *
		 * @param type_id The unique StrID of the metadata type.
		 * @param deserialize_func Function pointer to deserialize the metadata from bytes.
		 * @return true (always succeeds, asserts on duplicate registration).
		 */
		bool registerType(base::StrID type_id, BytesDeserializeFunc deserialize_func) {
			CORE_ASSERT(
				!registry_.contains(type_id),
				"Metadata type already registered: {}",
				type_id.strView()
			);
			registry_.put(type_id, DeserializerVariant{ deserialize_func });
			return true;
		}

		/**
		 * @brief Register a StrID metadata type with StrID deserializer.
		 *
		 * @param type_id The unique StrID of the metadata type.
		 * @param deserialize_func Function pointer to deserialize the metadata from StrID.
		 * @return true (always succeeds, asserts on duplicate registration).
		 */
		bool registerStrIDType(base::StrID type_id, StrIDDeserializeFunc deserialize_func) {
			CORE_ASSERT(
				!registry_.contains(type_id),
				"Metadata type already registered: {}",
				type_id.strView()
			);
			registry_.put(type_id, DeserializerVariant{ deserialize_func });
			return true;
		}

		/**
		 * @brief Get the deserializer variant for a type.
		 *
		 * @param type_id The type ID of the metadata.
		 * @return Optional containing the deserializer variant, or empty if not found.
		 */
		[[nodiscard]]
		base::Optional<DeserializerVariant> getDeserializer(base::StrID type_id) const {
			auto it = registry_.find(type_id);
			if (it == registry_.end()) return {};
			return it->second;
		}

		/**
		 * @brief Check if a type is registered.
		 */
		[[nodiscard]]
		bool isRegistered(base::StrID type_id) const {
			return registry_.contains(type_id);
		}

		/**
		 * @brief Check if a type is registered as StrID type.
		 */
		[[nodiscard]]
		bool isStrIDType(base::StrID type_id) const {
			auto it = registry_.find(type_id);
			if (it == registry_.end()) return false;
			return std::holds_alternative<StrIDDeserializeFunc>(it->second);
		}
	};

	/**
	 * @brief Data structure for extracted node metadata (for move operations).
	 */
	struct ExtractedNodeMetadata {
		internal::NodeID                                           node_id;
		base::HashMap<base::StrID, std::vector<Box<BaseMetadata>>> type_map;

		ExtractedNodeMetadata(
			internal::NodeID id, base::HashMap<base::StrID, std::vector<Box<BaseMetadata>>>&& map
		):
			  node_id(id),
			  type_map(std::move(map)) {}
	};

	/**
	 * @brief Storage for metadata associated with query nodes.
	 *
	 * This class manages the storage and retrieval of metadata attached to NodeIDs.
	 * Each NodeID can have multiple metadata values of different types.
	 * Multiple metadata values of the same type can also be attached to a single NodeID.
	 */
	class MetadataStorage final {
	private:
		/**
		 * @brief The storage structure:
		 * NodeID -> (StrID type id -> vector of metadata instances)
		 */
		using TypeMap     = base::HashMap<base::StrID, std::vector<Box<BaseMetadata>>>;
		using MetadataMap = base::HashMap<internal::NodeID, TypeMap>;

		MetadataMap storage_;

	public:
		MetadataStorage()                                  = default;
		MetadataStorage(const MetadataStorage&)            = delete;
		MetadataStorage(MetadataStorage&&)                 = default;
		MetadataStorage& operator=(const MetadataStorage&) = delete;
		MetadataStorage& operator=(MetadataStorage&&)      = default;

		/**
		 * @brief Add a metadata instance to a node.
		 *
		 * @tparam MetadataT The metadata type (must derive from BaseMetadata and have TYPE_ID)
		 * @tparam Args Argument types for constructing the metadata
		 * @param node_id The NodeID to attach metadata to
		 * @param args Arguments forwarded to MetadataT constructor
		 */
		template<typename MetadataT, typename... Args>
		requires std::derived_from<MetadataT, BaseMetadata>
		void addMetadata(internal::NodeID node_id, Args&&... args) {
			base::StrID type_id = MetadataT::TYPE_ID;

			// Create the metadata instance
			auto metadata = makeBox<MetadataT>(std::forward<Args>(args)...);

			// Get or create the node's metadata map
			if (!storage_.contains(node_id)) storage_.put(node_id, {});
			auto& node_map = storage_.at(node_id);

			// Get or create the type's vector
			if (!node_map.contains(type_id)) node_map.put(type_id, {});
			auto& type_vec = node_map.at(type_id);

			type_vec.push_back(std::move(metadata));
		}

		/**
		 * @brief Get all metadata of a specific type for a node.
		 *
		 * @tparam MetadataT The metadata type to retrieve
		 * @param node_id The NodeID to get metadata for
		 * @return std::vector<CRef<MetadataT>> References to all metadata of the given type.
		 *         Returns empty vector if no metadata of this type exists.
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		std::vector<CRef<MetadataT>> getMetadata(internal::NodeID node_id) const {
			std::vector<CRef<MetadataT>> result;
			base::StrID                  type_id = MetadataT::TYPE_ID;

			auto node_it = storage_.find(node_id);
			if (node_it == storage_.end()) return result;

			const auto& node_map = node_it->second;
			auto        type_it  = node_map.find(type_id);
			if (type_it == node_map.end()) return result;

			const auto& type_vec = type_it->second;
			result.reserve(type_vec.size());

			for (const auto& metadata_ptr: type_vec) {
				// Safe downcast - we know the type matches because we used type id as key
				const auto* typed_ptr = static_cast<const MetadataT*>(metadata_ptr.get());
				result.push_back(CRef<MetadataT>(typed_ptr));
			}

			return result;
		}

		/**
		 * @brief Check if a node has any metadata of a specific type.
		 *
		 * @tparam MetadataT The metadata type to check for
		 * @param node_id The NodeID to check
		 * @return true if the node has at least one metadata of this type
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		bool hasMetadata(internal::NodeID node_id) const {
			base::StrID type_id = MetadataT::TYPE_ID;

			auto node_it = storage_.find(node_id);
			if (node_it == storage_.end()) return false;

			const auto& node_map = node_it->second;
			auto        type_it  = node_map.find(type_id);
			if (type_it == node_map.end()) return false;

			return !type_it->second.empty();
		}

		/**
		 * @brief Get count of metadata of a specific type for a node.
		 *
		 * @tparam MetadataT The metadata type to count
		 * @param node_id The NodeID to check
		 * @return usize Number of metadata instances of this type
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		usize getMetadataCount(internal::NodeID node_id) const {
			base::StrID type_id = MetadataT::TYPE_ID;

			auto node_it = storage_.find(node_id);
			if (node_it == storage_.end()) return 0;

			const auto& node_map = node_it->second;
			auto        type_it  = node_map.find(type_id);
			if (type_it == node_map.end()) return 0;

			return type_it->second.size();
		}

		/**
		 * @brief Extract all metadata for a node, removing it from storage.
		 *
		 * @param node_id The NodeID to extract metadata for
		 * @return Optional<ExtractedNodeMetadata> The extracted data, or empty if node not found.
		 */
		[[nodiscard]]
		base::Optional<ExtractedNodeMetadata> extract(internal::NodeID node_id) {
			auto it = storage_.find(node_id);
			if (it == storage_.end()) return {};

			ExtractedNodeMetadata result(node_id, std::move(it->second));
			storage_.erase(it);
			return result;
		}

		/**
		 * @brief Emplace extracted metadata into storage.
		 *
		 * @param extracted The extracted metadata to emplace (must be rvalue).
		 */
		void emplace(ExtractedNodeMetadata&& extracted) {
			storage_.put(extracted.node_id, std::move(extracted.type_map));
		}

		/**
		 * @brief Clear all metadata for a specific node.
		 *
		 * @param node_id The NodeID to clear metadata for
		 */
		void clearNodeMetadata(internal::NodeID node_id) { storage_.erase(node_id); }

		/**
		 * @brief Clear all metadata storage.
		 */
		void clear() { storage_.clear(); }

		/**
		 * @brief Check if storage is empty.
		 * @return true if no metadata is stored
		 */
		[[nodiscard]]
		bool empty() const {
			return storage_.empty();
		}

		/**
		 * @brief Serialize all metadata to a byte vector.
		 *
		 * Format (optimized with type name table and StrID table):
		 *
		 * [type_table_size: u64]                    // Number of unique type names
		 * For each unique type (index = type_id):
		 *   [type_name_len: u64][type_name: bytes]
		 *
		 * [strid_table_size: u64]                   // Number of unique StrID values
		 * For each unique StrID (index = strid_idx):
		 *   [str_len: u64][str: bytes]
		 *
		 * [node_count: u64]
		 * For each node:
		 *   [NodeID: q_id (u64) + hash (Bit256)]
		 *   [type_count: u64]
		 *   For each type:
		 *     [type_id: u64]                        // Index in type table
		 *     [metadata_count: u64]
		 *     For each metadata:
		 *       If StrID type: [strid_idx: u64]     // Index in StrID table
		 *       Else: [data_size: u64][data: bytes]
		 *
		 * @return std::vector<std::byte> The serialized metadata storage.
		 */
		[[nodiscard]]
		std::vector<std::byte> serialize() const;

		/**
		 * @brief Deserialize metadata storage from a byte span.
		 *
		 * Uses MetadataRegistry to reconstruct concrete metadata types.
		 *
		 * @param data The serialized data
		 * @return MetadataStorage The deserialized storage
		 */
		static MetadataStorage deserialize(std::span<const std::byte> data);
	};

}  // namespace query
