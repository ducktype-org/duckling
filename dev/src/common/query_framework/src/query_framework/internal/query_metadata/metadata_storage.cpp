/**
 * @file metadata_storage.cpp
 * @brief Implementation of metadata storage serialization/deserialization.
 */

#include "metadata_storage.hpp"

#include "metadata_registry.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

#include <span>

namespace query::internal {

	base::Optional<ExtractedNodeMetadata> MetadataStorage::extract(NodeID node_id) {
		auto extracted = storage.extract(node_id);

		if (!extracted.has_value()) {
			/* NodeID not found, return empty optional */
			return base::Optional<ExtractedNodeMetadata>{};
		} else {
			/* We have to convert ConHashMap to StableHashMap for the extracted data */
			base::StableHashMap<BaseMetadata::TypeID, std::vector<Box<BaseMetadata>>> extracted_map;

			for (auto& [type_id, metadata_vec]: extracted.value())
				extracted_map.put(type_id, std::move(metadata_vec));

			return ExtractedNodeMetadata{ node_id, std::move(extracted_map) };
		}
	}

	void MetadataStorage::emplace(ExtractedNodeMetadata&& extracted) {
		/** @brief convert StableHashMap back to ConHashMap for storage */
		TypeMap type_map;
		for (auto& [type_id, metadata_vec]: extracted.type_map)
			type_map.put(type_id, std::move(metadata_vec));
		extracted.type_map.clear();

		storage.put(std::move(extracted).node_id, std::move(type_map));
	}

	void MetadataStorage::maybeEmplace(ExtractedNodeMetadata&& extracted) {
		/** @brief convert StableHashMap back to ConHashMap for storage */
		TypeMap type_map;
		for (auto& [type_id, metadata_vec]: extracted.type_map)
			type_map.put(type_id, std::move(metadata_vec));
		extracted.type_map.clear();

		storage.maybePut(std::move(extracted).node_id, std::move(type_map));
	}

	void MetadataStorage::clearNodeMetadata(NodeID node_id) { storage.erase(node_id); }

	bool MetadataStorage::empty() const { return storage.size() == 0; }

	::ser::Errc MetadataStorage::writeInto(MetadataOut& ar, const MetadataStorage& self) {
		/* First pass: collect all unique type names and assign IDs */
		std::vector<base::StrID>              type_table; /* ID -> StrID */
		base::StableHashMap<base::StrID, u64> type_to_id; /* StrID -> ID */

		/* Also collect all unique StrID values from StrID-type metadata */
		std::vector<base::StrID>              strid_table; /* ID -> StrID value */
		base::StableHashMap<base::StrID, u64> strid_to_id; /* StrID value -> ID */

		for (const auto& [node_id, type_map]: self.storage) {
			for (const auto& [type_id, metadata_vec]: type_map) {
				/* Collect type names */
				if (!type_to_id.contains(type_id)) {
					u64 new_id = type_table.size();
					type_table.push_back(type_id);
					type_to_id.put(type_id, new_id);
				}

				/* Collect StrID values from StrID-type metadata */
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

		/* Both tables, then the node count */
		if (const auto e = ar(type_table, strid_table, static_cast<u64>(self.storage.size()));
		    e != ::ser::Errc::Ok)
			return e;

		for (const auto& [node_id, type_map]: self.storage) {
			if (const auto e = ar(node_id, static_cast<u64>(type_map.size())); e != ::ser::Errc::Ok)
				return e;

			for (const auto& [type_id, metadata_vec]: type_map) {
				if (const auto e = ar(
						*type_to_id.atMaybe(type_id).value(), static_cast<u64>(metadata_vec.size())
					);
				    e != ::ser::Errc::Ok)
					return e;

				/*
				 * Which of the two shapes an instance takes is NOT on the wire: it is a
				 * property of the type, and the type is already there as its index into the
				 * table above. The reader asks the MetadataRegistry the same question.
				 */
				for (const auto& metadata: metadata_vec) {
					if (metadata->usesStrIDTable()) {
						/** @brief For StrID types, only the index in the StrID table travels */
						base::StrID str_value = metadata->getStrIDValue();
						if (const auto e = ar(*strid_to_id.atMaybe(str_value).value());
						    e != ::ser::Errc::Ok)
							return e;
					} else {
						/* Everything else writes itself, into this same stream */
						if (const auto e = metadata->serWrite(ar); e != ::ser::Errc::Ok) return e;
					}
				}
			}
		}

		return ::ser::Errc::Ok;
	}

	::ser::Errc MetadataStorage::readFrom(MetadataIn& ar, MetadataStorage& self) {
		std::vector<base::StrID> type_table;
		std::vector<base::StrID> strid_table;
		u64                      node_count = 0;
		if (const auto e = ar(type_table, strid_table, node_count); e != ::ser::Errc::Ok) return e;

		/*
		 * Nothing is reserved from a count that came off the wire: every entry below reads
		 * at least one byte, so a damaged count runs out of stream instead of memory.
		 *
		 * Everything that came off the wire is checked with a CODE rather than an assert,
		 * the two index bounds included: a damaged cache has to be reportable, and the caller
		 * then compiles without one.
		 */
		for (u64 i = 0; i < node_count; ++i) {
			/* A NodeID has no default constructor, so it is built rather than filled */
			const auto node_id = ::ser::subMake<NodeID>(ar);

			/* The NodeID has to be registered and preserved in the graph */
			if (!node_id.q_id.registered() || !node_id.q_id.getData().tags.preserve_in_graph)
				return ::ser::Errc::InvalidValue;

			u64 type_count = 0;
			if (const auto e = ar(type_count); e != ::ser::Errc::Ok) return e;

			for (u64 j = 0; j < type_count; ++j) {
				u64 type_index     = 0;
				u64 metadata_count = 0;
				if (const auto e = ar(type_index, metadata_count); e != ::ser::Errc::Ok) return e;

				if (type_index >= type_table.size()) return ::ser::Errc::InvalidValue;
				base::StrID type_id = type_table[type_index];

				/* A stream naming a type this build does not have is a stale cache, not a bug */
				if (!MetadataRegistry::instance().isRegistered(type_id))
					return ::ser::Errc::InvalidValue;

				/* Get deserializer variant */
				const auto& deserializer = MetadataRegistry::instance().getDeserializer(type_id);

				/*
				 * The registry decides which shape to read, exactly as it decided which shape
				 * to write - so nothing per instance says it again.
				 */
				for (u64 k = 0; k < metadata_count; ++k) {
					base::Optional<Box<BaseMetadata>> metadata_opt;

					variant_match(deserializer) {
						variant_case(StrIDDeserializeFunc, func) {
							u64 strid_index = 0;
							if (const auto e = ar(strid_index); e != ::ser::Errc::Ok) return e;
							if (strid_index >= strid_table.size()) return ::ser::Errc::InvalidValue;
							metadata_opt = func(strid_table[strid_index]);
						}
						variant_case(ArchiveDeserializeFunc, func) { metadata_opt = func(ar); }
					}

					/* Add to storage */
					if (!self.storage.contains(node_id)) self.storage.put(node_id, TypeMap{});
					auto& node_map = *self.storage.atMaybe(node_id).value();
					if (!node_map.contains(type_id))
						node_map.put(type_id, std::vector<Box<BaseMetadata>>{});

					node_map.atMaybe(type_id).value()->push_back(std::move(metadata_opt).value());
				}
			}
		}

		return ::ser::Errc::Ok;
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

} /* namespace query */
