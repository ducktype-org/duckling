/**
 * @file metadata_storage.cpp
 * @brief Implementation of metadata storage serialization/deserialization.
 */

#include "metadata_storage.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <cstring>

namespace query::internal {

	namespace {
		// Helper to write a u64 to a byte vector
		void writeU64(std::vector<std::byte>& out, u64 value) {
			auto* ptr = reinterpret_cast<const std::byte*>(&value);
			out.insert(out.end(), ptr, ptr + sizeof(u64));
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
			auto* ptr = reinterpret_cast<const std::byte*>(str.data());
			out.insert(out.end(), ptr, ptr + str.size());
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
		auto it = storage.atMaybe(node_id);
		if (!it.has_value()) return {};

		ExtractedNodeMetadata result(node_id, std::move(*it.value()));
		storage.erase(node_id);
		return result;
	}

	void MetadataStorage::emplace(ExtractedNodeMetadata&& extracted) {
		storage.put(std::move(extracted).node_id, std::move(extracted.type_map));
	}

	void MetadataStorage::clearNodeMetadata(NodeID node_id) { storage.erase(node_id); }

	void MetadataStorage::clear() { storage.clear(); }

	bool MetadataStorage::empty() const { return storage.size() == 0; }

	std::vector<std::byte> MetadataStorage::serialize() const {
		std::vector<std::byte> result;

		// First pass: collect all unique type names and assign IDs
		std::vector<base::StrID>        type_table;  // ID -> StrID
		base::HashMap<base::StrID, u64> type_to_id;  // StrID -> ID

		// Also collect all unique StrID values from StrID-type metadata
		std::vector<base::StrID>        strid_table;  // ID -> StrID value
		base::HashMap<base::StrID, u64> strid_to_id;  // StrID value -> ID

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
			auto* hash_ptr = reinterpret_cast<const std::byte*>(&node_id.hash.val);
			result.insert(result.end(), hash_ptr, hash_ptr + sizeof(base::Bit256));

			// Write type count
			writeU64(result, type_map.size());

			for (const auto& [type_id, metadata_vec]: type_map) {
				// Write type ID (index in type table)
				writeU64(result, type_to_id.at(type_id));

				// Write metadata count
				writeU64(result, metadata_vec.size());

				for (const auto& metadata: metadata_vec) {
					if (metadata->usesStrIDTable()) {
						// For StrID types, write only the index in StrID table
						base::StrID str_value = metadata->getStrIDValue();
						writeU64(result, strid_to_id.at(str_value));
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
		MetadataStorage storage;

		if (data.empty()) return storage;

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
				auto deserializer_opt = MetadataRegistry::instance().getDeserializer(type_id);
				CORE_ASSERT(
					deserializer_opt.has_value(),
					"Unknown metadata type_id: {} (not registered)",
					type_id.strView()
				);
				const auto& deserializer = deserializer_opt.value();

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

}  // namespace query
