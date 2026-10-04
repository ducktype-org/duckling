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

#include <concurrent/base/collections/hash_map.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>
#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <string_id/string_id.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace query::internal {

	/**
	 * @brief The one archive pair the whole metadata format goes through.
	 *
	 * A metadata instance writes itself through a virtual, and a virtual cannot be a
	 * template - so the archive it writes into has to be one concrete type. Pinning it here
	 * is what makes the format a SINGLE stream: no instance ever gets a buffer of its own,
	 * and a nested blob with its own length prefix does not exist.
	 */
	using MetadataOut = ::ser::Out<std::vector<std::byte>>;
	using MetadataIn  = ::ser::In<>;

	/**
	 * @brief Abstract base class for all metadata types.
	 *
	 * All metadata types must inherit from this class and write themselves through
	 * serWrite(). Reading is the mirror image and cannot be virtual - there is no object
	 * yet - so a derived type registers a factory with the MetadataRegistry instead; the
	 * DECLARE_METADATA macro does both.
	 *
	 * @note Use the DECLARE_METADATA macro to create new metadata types,
	 * which handles the boilerplate automatically.
	 *
	 * User code should not directly inherit from BaseMetadata.
	 * This is for internal use by the metadata system only.
	 */
	struct BaseMetadata {
		using TypeID = base::StrID;

		virtual ~BaseMetadata() = default;

		/**
		 * @brief Writes the metadata's value into the metadata stream.
		 * @note Named after the `ser` hook it plays the part of, with the archive pinned for
		 * the reason given on MetadataOut.
		 * @return ser::Errc::Ok, or the code of the first field that failed.
		 */
		[[nodiscard]]
		virtual ::ser::Errc serWrite(MetadataOut& ar) const
			= 0;

		/**
		 * @brief Get the type ID (StrID) of this metadata type.
		 * Used for serialization and runtime type identification.
		 * @return TypeID The unique type identifier.
		 */
		[[nodiscard]]
		virtual TypeID getTypeID() const
			= 0;

		/**
		 * @brief Check if this metadata type uses the StrID table for optimized serialization.
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
			CORE_PANIC("getStrIDValue() called on non-StrID metadata type");
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Pretty print the metadata for debugging.
		 * @param os The output stream to print to.
		 */
		virtual void prettyPrint(std::ostream& os) const {
			// Print that pretty print is not implemented for this type
			os << "BaseMetadata (type: " << getTypeID().strView()
			   << ") - prettyPrint not implemented.\n";
		}
	};

	/**
	 * @brief Data structure for extracted node metadata (for move operations).
	 * This is used when merging metadata from a previous compilation during graph merging.
	 */
	struct ExtractedNodeMetadata final {
		NodeID                                                                    node_id;
		base::StableHashMap<BaseMetadata::TypeID, std::vector<Box<BaseMetadata>>> type_map;

		ExtractedNodeMetadata(
			NodeID                                                                      id,
			base::StableHashMap<BaseMetadata::TypeID, std::vector<Box<BaseMetadata>>>&& map
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
		 * NodeID -> (TypeID -> vector of metadata instances)
		 */
		using TypeID = BaseMetadata::TypeID;

		using TypeMap = concurrent::ConHashMap<TypeID, std::vector<Box<BaseMetadata>>>;

		/**
		 * @brief The main storage map: NodeID -> TypeMap
		 * For each NodeID, it stores a map of TypeID to vectors of metadata instances.
		 * This allows multiple metadata instances of the same type per NodeID.
		 */
		using MetadataMap = concurrent::ConHashMap<NodeID, TypeMap>;

		/**
		 * @brief This actually stores the metadata.
		 */
		MetadataMap storage;

	public:
		MetadataStorage()                           = default;
		MetadataStorage(MetadataStorage&&) noexcept = default;

		MetadataStorage(const MetadataStorage&)            = delete;
		MetadataStorage& operator=(MetadataStorage&&)      = delete;
		MetadataStorage& operator=(const MetadataStorage&) = delete;

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
		void addMetadata(NodeID node_id, Args&&... args) {
			TypeID type_id = MetadataT::TYPE_ID;

			// Create the metadata instance
			auto metadata = makeBox<MetadataT>(std::forward<Args>(args)...);

			// Get or create the node's metadata map
			storage.maybePutAndUpdate(node_id, {}, [&](Ref<TypeMap> node_map) {
				// Get or create the type's vector
				node_map->maybePutAndUpdate(
					type_id,
					{},
					[&metadata](Ref<std::vector<Box<BaseMetadata>>> metadata_vector) {
						metadata_vector->push_back(std::move(metadata));
					}
				);
			});
		}

		/**
		 * @brief Add a metadata instance to a node only if no metadata of this type exists.
		 *
		 * Use this method when you know that for a given node you want only one metadata
		 * instance of this type, but the same code path might be executed multiple times
		 *
		 * This is more efficient than checking hasMetadata() + addMetadata() separately,
		 * and ensures atomicity of the check-and-add operation.
		 *
		 * @tparam MetadataT The metadata type (must derive from BaseMetadata and have TYPE_ID)
		 * @tparam Args Argument types for constructing the metadata
		 * @param node_id The NodeID to attach metadata to
		 * @param args Arguments forwarded to MetadataT constructor
		 * @return true if metadata was added, false if metadata of this type already exists
		 */
		template<typename MetadataT, typename... Args>
		requires std::derived_from<MetadataT, BaseMetadata>
		bool addMetadataIfNotExists(NodeID node_id, Args&&... args) {
			TypeID type_id = MetadataT::TYPE_ID;

			// Create the metadata instance
			auto metadata = makeBox<MetadataT>(std::forward<Args>(args)...);

			bool was_added = false;

			// Get or create the node's metadata map
			storage.maybePutAndUpdate(node_id, {}, [&](Ref<TypeMap> node_map) {
				// Get or create the type's vector
				node_map->maybePutAndUpdate(
					type_id,
					{},
					[&metadata, &was_added](Ref<std::vector<Box<BaseMetadata>>> metadata_vector) {
						if (metadata_vector->empty()) {
							// no metadata exists, so we add
							metadata_vector->push_back(std::move(metadata));
							was_added = true;
						}
					}
				);
			});

			return was_added;
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
		std::vector<CRef<MetadataT>> getMetadata(NodeID node_id) const {
			std::vector<CRef<MetadataT>> result;
			TypeID                       type_id = MetadataT::TYPE_ID;

			storage.maybeCallOn(node_id, [&](CRef<TypeMap> node_map) {
				node_map->maybeCallOn(
					type_id,
					[&result](CRef<std::vector<Box<BaseMetadata>>> type_vec) {
						result.reserve(type_vec->size());
						for (const auto& metadata_ptr: *type_vec) {
							// Safe downcast - we know the type matches because we used type id as key
							const auto* typed_ptr
								= static_cast<const MetadataT*>(metadata_ptr.get());
							result.push_back(CRef<MetadataT>(typed_ptr));
						}
					}
				);
			});

			return result;
		}

		/**
		 * @brief Structure to hold metadata with its associated NodeID.
		 * @tparam MetadataT The metadata type.
		 */
		template<typename MetadataT>
		struct MetadataInfo final {
			NodeID          node_id;
			CRef<MetadataT> value;
		};

		/**
		 * @brief Get all metadata of a specific type from all nodes.
		 *
		 * Efficiently iterates through all nodes once, collecting metadata of the given type.
		 *
		 * @tparam MetadataT The metadata type to retrieve
		 * @return std::vector<MetadataInfo<MetadataT>> Metadata with associated NodeIDs.
		 *         Returns empty vector if no metadata of this type exists.
		 */
		template<typename MetadataT>
		requires std::derived_from<MetadataT, BaseMetadata> [[nodiscard]]
		std::vector<MetadataInfo<MetadataT>> getMetadataFromAllNodes() const {
			std::vector<MetadataInfo<MetadataT>> result;
			TypeID                               type_id = MetadataT::TYPE_ID;

			// Single pass through all nodes
			// note that iteration here locks storage
			for (const auto& [node_id, node_map]: storage) {
				node_map.maybeCallOn(
					type_id,
					[&result, node_id](CRef<std::vector<Box<BaseMetadata>>> type_vec) {
						for (const auto& metadata_ptr: *type_vec) {
							// Safe downcast - we know the type matches because we used type id as key
							const auto* typed_ptr
								= static_cast<const MetadataT*>(metadata_ptr.get());
							result.push_back(MetadataInfo<MetadataT>{
								.node_id = node_id,
								.value   = CRef<MetadataT>(typed_ptr),
							});
						}
					}
				);
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
		bool hasMetadata(NodeID node_id) const {
			TypeID type_id = MetadataT::TYPE_ID;

			bool result = false;

			storage.maybeCallOn(node_id, [&](CRef<TypeMap> node_map) {
				node_map->maybeCallOn(
					type_id,
					[&result](CRef<std::vector<Box<BaseMetadata>>> type_vec) {
						result = !type_vec->empty();  // this will set result to true only if there
					                                  // is at least one metadata of this type
					}
				);
			});


			return result;
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
		usize getMetadataCount(NodeID node_id) const {
			TypeID type_id = MetadataT::TYPE_ID;

			usize result = 0;

			storage.maybeCallOn(node_id, [&](CRef<TypeMap> node_map) {
				node_map->maybeCallOn(
					type_id,
					[&result](CRef<std::vector<Box<BaseMetadata>>> type_vec) {
						result = type_vec->size();
					}
				);
			});

			return result;
		}

		/**
		 * @brief Extract all metadata for a node, removing it from storage.
		 *
		 * @param node_id The NodeID to extract metadata for
		 * @return Optional<ExtractedNodeMetadata> The extracted data, or empty if node not found.
		 */
		[[nodiscard]]
		base::Optional<ExtractedNodeMetadata> extract(NodeID node_id);

		/**
		 * @brief Emplace extracted metadata into storage.
		 *
		 * @param extracted The extracted metadata to emplace (must be rvalue).
		 */
		void emplace(ExtractedNodeMetadata&& extracted);

		/**
		 * @brief Maybe emplace extracted metadata into storage if node does not already exist.
		 * This is useful when merging input nodes form prev compilation, where the node may already
		 * exist in the current graph.
		 * @param extracted The extracted metadata to emplace (must be rvalue).
		 */
		void maybeEmplace(ExtractedNodeMetadata&& extracted);

		/**
		 * @brief Clear all metadata for a specific node.
		 *
		 * @param node_id The NodeID to clear metadata for
		 */
		void clearNodeMetadata(NodeID node_id);

		/**
		 * @brief Check if storage is empty.
		 * @return true if no metadata is stored
		 */
		[[nodiscard]]
		bool empty() const;

		/**
		 * @brief `ser` hook: writes the type table, the StrID table, and then every node.
		 *
		 *
		 * A hand-written pair rather than the automatic field walk, because the format is not
		 * this class's fields
		 *
		 * The two tables are what keeps the format compact: a type name and a repeated StrID
		 * value are written once and referenced by index afterwards.
		 *
		 * \parallel Writing should not race, but might behave weirdly if metadata is being
		 * concurrently modified, as there is no large lock in place. It should be used in a
		 * context where we can guarantee no concurrent modifications.
		 */
		static ::ser::Errc serWrite(::ser::Writer auto& ar, const MetadataStorage& self) {
			static_assert(
				std::same_as<std::remove_cvref_t<decltype(ar)>, MetadataOut>,
				"query: a MetadataStorage can only be written through "
				"ser::Out<std::vector<std::byte>>, because every metadata instance writes itself "
				"through a virtual and a virtual cannot be a template. Serialize into a "
				"std::vector<std::byte>."
			);
			return writeInto(ar, self);
		}

		/**
		 * @brief `ser` hook: reads back what serWrite wrote, one instance at a time through
		 * the MetadataRegistry.
		 */
		static ::ser::Errc serRead(::ser::Reader auto& ar, MetadataStorage& self) {
			static_assert(
				std::same_as<std::remove_cvref_t<decltype(ar)>, MetadataIn>,
				"query: a MetadataStorage can only be read through ser::In<> - see the note on "
				"serWrite."
			);
			return readFrom(ar, self);
		}

		/**
		 * @brief Pretty print the metadata storage for debugging.
		 * \parallel This method should not race, but might behave weirdly if metadata is being
		 * concurrently modified during printing, as there is no large lock in place. It should be
		 * used in a context where we can guarantee no concurrent modifications.
		 * @param os The output stream to print to.
		 */
		void prettyPrint(std::ostream& os) const;

	private:
		/**
		 * @brief The body of the serWrite hook, with the archive at its concrete type.
		 * @note The hook above is a template only so that `ser` can see it from both
		 * directions
		 */
		static ::ser::Errc writeInto(MetadataOut& ar, const MetadataStorage& self);

		/** @brief The body of the serRead hook, with the archive at its concrete type. */
		static ::ser::Errc readFrom(MetadataIn& ar, MetadataStorage& self);
	};

}  // namespace query
