/**
 * @file metadata_storage.cpp
 * @brief Implementation of metadata storage serialization/deserialization.
 * @TODO: #1942 - Move/Refactor this
 */

#include "metadata_storage.hpp"

#include "metadata_registry.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <cstring>
#include <span>

namespace query::internal {

	namespace {
		// Helper to write a u64 to a byte vector
		void writeU64(std::vector<std::byte>& out, u64 value) {
			auto bytes = std::as_bytes(std::span<const u64>(&value, 1));
			out.insert(out.end(), bytes.begin(), bytes.end());
		}

		// Helper to read a u64 from a byte span, advancing the offset
		u64 readU64(std::span<const std::byte> data, usize& offset) {
			u64 value = 0;
			std::memcpy(&value, data.data() + offset, sizeof(u64));
			offset += sizeof(u64);
			return value;
		}

		// Helper to write a string to a byte vector (length-prefixed)
		void writeString(std::vector<std::byte>& out, std::string_view str) {
			writeU64(out, str.size());
			auto bytes = std::as_bytes(std::span(str.data(), str.size()));
			out.insert(out.end(), bytes.begin(), bytes.end());
		}

		// Helper to read a string from a byte span, advancing the offset
		std::string_view readString(std::span<const std::byte> data, usize& offset) {
			u64   len = readU64(data, offset);
			auto* ptr = reinterpret_cast<const char*>(data.data() + offset);
			offset += len;
			return { ptr, len };
		}

		// Helper to write bytes to a byte vector (length-prefixed)
		void writeBytes(std::vector<std::byte>& out, std::span<const std::byte> bytes) {
			writeU64(out, bytes.size());
			out.insert(out.end(), bytes.begin(), bytes.end());
		}

		// Helper to read bytes from a byte span, advancing the offset
		std::span<const std::byte> readBytes(std::span<const std::byte> data, usize& offset) {
			u64  len    = readU64(data, offset);
			auto result = data.subspan(offset, len);
			offset += len;
			return result;
		}
	}  // namespace

	base::Optional<ExtractedNodeMetadata> MetadataStorage::extract(NodeID node_id) {
		auto extracted = storage.extract(node_id);

		if (!extracted.has_value()) {
			// NodeID not found, return empty optional
			return base::Optional<ExtractedNodeMetadata>{};
		} else {
			// We have to convert ConHashMap to StableHashMap for the extracted data
			base::StableHashMap<BaseMetadata::TypeID, std::vector<Box<BaseMetadata>>> extracted_map;

			for (auto& [type_id, metadata_vec]: extracted.value())
				extracted_map.put(type_id, std::move(metadata_vec));

			return ExtractedNodeMetadata{ node_id, std::move(extracted_map) };
		}
	}

	void MetadataStorage::emplace(ExtractedNodeMetadata&& extracted) {
		// convert StableHashMap back to ConHashMap for storage
		TypeMap type_map;
		for (auto& [type_id, metadata_vec]: extracted.type_map)
			type_map.put(type_id, std::move(metadata_vec));
		extracted.type_map.clear();

		// \parallel This runs on query worker threads while the previous graph is merged into the
		// current one (QueryState::mergePreviousGraphIntoCurrentGraph). The same node can already
		// have metadata in the current storage produced by a concurrent re-execution of that node
		// (MetadataStorage::addMetadata), so a plain asserting put() would trip
		// "Key already exists in StableHashMap". Freshly executed metadata is authoritative, so we
		// keep the existing entry and skip restoring the (identical for a green node / stale for a
		// recomputed one) previous metadata. maybePut() does this atomically under the shard lock.
		storage.maybePut(std::move(extracted).node_id, std::move(type_map));
	}

	void MetadataStorage::clearNodeMetadata(NodeID node_id) { storage.erase(node_id); }

	bool MetadataStorage::empty() const { return storage.size() == 0; }

	std::vector<std::byte> MetadataStorage::serialize() const {
		std::vector<std::byte> result;

		// First pass: collect all unique type names and assign IDs
		std::vector<base::StrID>              type_table;  // ID -> StrID
		base::StableHashMap<base::StrID, u64> type_to_id;  // StrID -> ID

		// Also collect all unique StrID values from StrID-type metadata
		std::vector<base::StrID>              strid_table;  // ID -> StrID value
		base::StableHashMap<base::StrID, u64> strid_to_id;  // StrID value -> ID

		for (const auto& [node_id, type_map]: storage) {
			for (const auto& [type_id, metadata_vec]: type_map) {
				// Collect type names
				if (!type_to_id.contains(type_id)) {
					u64 new_id = type_table.size();
					type_table.push_back(type_id);
					type_to_id.put(type_id, new_id);
				}

				// Collect StrID values from StrID-type metadata
				for (const auto& metadata: metadata_vec) {
					if (metadata->usesStrIDTable()) {
						base::StrID str_value = metadata->getStrIDValue();
						if (!strid_to_id.contains(str_value)) {
							u64 new_id = strid_table.size();
							strid_table.push_back(str_value);
							strid_to_id.put(str_value, new_id);
						}
					}
				}
			}
		}

		// Write type table
		writeU64(result, type_table.size());
		for (const auto& type_id: type_table) writeString(result, type_id.strView());

		// Write StrID table
		writeU64(result, strid_table.size());
		for (const auto& str_id: strid_table) writeString(result, str_id.strView());

		// Write node count
		writeU64(result, storage.size());

		for (const auto& [node_id, type_map]: storage) {
			// Serialize NodeID: QueryID as u64 + KeyHash as Bit256
			writeU64(result, node_id.q_id.asInt());

			// Write Bit256 (4 x u64)
			auto hash_bytes = std::as_bytes(std::span<const base::Bit256>(&node_id.hash.val, 1));
			result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());

			// Write type count
			writeU64(result, type_map.size());

			for (const auto& [type_id, metadata_vec]: type_map) {
				// Write type ID (index in type table)
				writeU64(result, *type_to_id.atMaybe(type_id).value());

				// Write metadata count
				writeU64(result, metadata_vec.size());

				for (const auto& metadata: metadata_vec) {
					if (metadata->usesStrIDTable()) {
						// For StrID types, write only the index in StrID table
						base::StrID str_value = metadata->getStrIDValue();
						writeU64(result, *strid_to_id.atMaybe(str_value).value());
					} else {
						// For regular types, serialize and write bytes
						auto serialized = metadata->serialize();
						writeBytes(result, serialized);
					}
				}
			}
		}

		return result;
	}

	MetadataStorage MetadataStorage::deserialize(std::span<const std::byte> data) {
		if (data.empty()) return MetadataStorage{};

		MetadataStorage storage;

		usize offset = 0;

		// Read type table
		u64                      type_table_size = readU64(data, offset);
		std::vector<base::StrID> type_table;
		type_table.reserve(type_table_size);

		for (u64 i = 0; i < type_table_size; ++i) {
			std::string_view type_name_view = readString(data, offset);
			type_table.emplace_back(std::string{ type_name_view });
		}

		// Read StrID table
		u64                      strid_table_size = readU64(data, offset);
		std::vector<base::StrID> strid_table;
		strid_table.reserve(strid_table_size);

		for (u64 i = 0; i < strid_table_size; ++i) {
			std::string_view str_view = readString(data, offset);
			strid_table.emplace_back(std::string{ str_view });
		}

		// Read node count
		u64 node_count = readU64(data, offset);

		for (u64 i = 0; i < node_count; ++i) {
			// Read NodeID
			u64 q_id_val = readU64(data, offset);

			base::Bit256 hash_val;
			std::memcpy(&hash_val, data.data() + offset, sizeof(base::Bit256));
			offset += sizeof(base::Bit256);

			// Reconstruct NodeID
			NodeID node_id{ QueryID{ q_id_val }, KeyHash{ .val = hash_val } };

			// Assert that NodeID is registered and has preserve_in_graph = true
			CORE_ASSERT(
				node_id.q_id.registered() && node_id.q_id.getData().tags.preserve_in_graph,
				"NodeID must be registered and preserved in graph"
			);

			// Read type count
			u64 type_count = readU64(data, offset);

			for (u64 j = 0; j < type_count; ++j) {
				// Read type ID (index in type table)
				u64 type_idx = readU64(data, offset);
				CORE_ASSERT(type_idx < type_table.size(), "Invalid type index in serialized data");
				base::StrID type_id = type_table[type_idx];

				// Get deserializer variant
				const auto& deserializer = MetadataRegistry::instance().getDeserializer(type_id);

				// Read metadata count
				u64 metadata_count = readU64(data, offset);

				for (u64 k = 0; k < metadata_count; ++k) {
					base::Optional<Box<BaseMetadata>> metadata_opt;

					variant_match(deserializer) {
						variant_case(StrIDDeserializeFunc, func) {
							// Read StrID index and look up in table
							u64 strid_idx = readU64(data, offset);
							CORE_ASSERT(
								strid_idx < strid_table.size(),
								"Invalid StrID index in serialized data"
							);
							metadata_opt = func(strid_table[strid_idx]);
						}
						variant_case(BytesDeserializeFunc, func) {
							// Read metadata bytes
							auto metadata_bytes = readBytes(data, offset);
							metadata_opt        = func(metadata_bytes);
						}
					}

					// Add to storage
					if (!storage.storage.contains(node_id)) storage.storage.put(node_id, TypeMap{});
					auto& node_map = *storage.storage.atMaybe(node_id).value();
					if (!node_map.contains(type_id))
						node_map.put(type_id, std::vector<Box<BaseMetadata>>{});

					node_map.atMaybe(type_id).value()->push_back(std::move(metadata_opt).value());
				}
			}
		}

		return storage;
	}

	void MetadataStorage::prettyPrint(std::ostream& os) const {
		os << "MetadataStorage with " << storage.size() << " nodes:\n";
		for (const auto& [node_id, type_map]: storage) {
			os << "  NodeID(q_id=" << node_id.q_id.getData().name << ", hash=" << node_id.hash.val
			   << "):\n";
			for (const auto& [type_id, metadata_vec]: type_map) {
				os << "    TypeID: " << type_id.strView() << " (" << metadata_vec.size()
				   << " instances)\n";
				for (const auto& metadata: metadata_vec) {
					os << "      - ";
					metadata->prettyPrint(os);
				}
			}
		}
	}

}  // namespace query
