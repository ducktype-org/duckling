#pragma once

#include <filesystem/file.hpp>
#include <hashing/hash.hpp>

#include "base/box.hpp"
#include "base/exceptions.hpp"
#include "base/ints.hpp"
#include "base/ref.hpp"
#include "base/stable_container.hpp"
#include "base/string_id.hpp"
#include "base/strongly_typed_id.hpp"

#include <exception>
#include <expected>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

/*
Idea:
    1. ArtMan really creates artifacts and contains all the data.
    2. Artifacts Manage themselves and it's a tree structure by default. It's a little better
because it maps better to the filesystem.

Variants vs inheritance:
    1. Variants are more flexible, but require more code?.
    2. Inheritance imposes a more concrete base
*/

namespace artman {
	/**
	 * @brief A collection of bytes.
	 * @note It cannot be read-only, because `std::vector<const byte>` does not compile.
	 */
	using Bytes = std::vector<byte>;

	STRONG_TYPEDEF_ID_DIRECT_CREATION(ArtifactCollectionId);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(ArtifactInnerId);

	struct ArtifactId {
		const ArtifactCollectionId PARENT_ID;
		const ArtifactInnerId      INNER_ID;

		friend constexpr auto hashDecompose(const ArtifactId& t) noexcept {
			return std::tie(t.PARENT_ID, t.INNER_ID);
		}
	};

	class ArtifactCollection;

	struct FileArtifact {
		const Ref<ArtifactCollection> PARENT;
		const ArtifactId              ID;

		const fs::FilePath FILE;
	};

	struct BlobArtifact {
		const Ref<ArtifactCollection> PARENT;
		const ArtifactId              ID;

		void                                        setData(const byte* ptr, usize n_bytes);
		[[nodiscard]] std::pair<const byte*, usize> getData() const;
	};

	enum class CollectionType { SingleFile, Blob };

	/**
	 * @brief Represents an artifact group. (It basically maps to a directory.).
	 * All collections can be modified independently (because as of now there is no state hashing).
	 */
	class ArtifactCollection {
		static constexpr i64 PROTOCOL_VERSION = 1;

		/**
		 * @brief Represents how
		 */
		std::vector<FileArtifact>                              file_artifacts;
		std::vector<BlobArtifact>                              blob_artifacts;
		base::HashMap<ArtifactId, Box<Bytes>, hashing::Hash<>> blob_data;

		std::vector<Box<ArtifactCollection>> sub_collections;  /// Box, because we need stable refs.

		ArtifactId nextId() const {
			// It's the simplest way to allow inserting new artifacts after loading the state from a
			// disk.
			return {
				.PARENT_ID = ID,
				.INNER_ID  = ArtifactInnerId(file_artifacts.size() + blob_artifacts.size()),
			};
		}

	public:
		const fs::FilePath PATH;

		const ArtifactCollectionId ID;

		const CollectionType COLLECTION_TYPE;

		ArtifactCollection(
			fs::FilePath root, CollectionType collection_type = CollectionType::SingleFile
		):
			  PATH(std::move(root)),
			  ID(0),
			  COLLECTION_TYPE(collection_type) {}

		CRef<ArtifactCollection> newCollection(base::StrID collection_name, CollectionType grouping_mode) {
			auto new_path = PATH.createDirectoryIn(collection_name.strView());
			sub_collections.push_back(Box<ArtifactCollection>::fromPointer(
				new ArtifactCollection(new_path, grouping_mode)
			));
			return sub_collections.back().ref();
		}

		FileArtifact newFileArtifact() {
			file_artifacts.push_back(FileArtifact{
				.PARENT = this,
				.ID     = nextId(),
				.FILE   = PATH,
			});
			return file_artifacts.back();
		}

		BlobArtifact newBlobArtifact() {
			blob_artifacts.push_back(BlobArtifact{
				.PARENT = this,
				.ID     = nextId(),
			});
			return blob_artifacts.back();
		}

		void setBlobData(BlobArtifact blob, const byte* ptr, usize n_bytes) {
			CORE_ASSERT(blob.ID.PARENT_ID == ID, "Blob does not belong to this collection");
			blob_data[blob.ID] = makeBox<Bytes>();
			if (blob_data.contains(blob.ID)) *blob_data[blob.ID] = Bytes(n_bytes, *ptr);
		}

		template<class T>
		requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
		void setBlobData(BlobArtifact blob, const T& data) {
			setBlobData(blob, &data, sizeof(data));
		}

		void flush() {
			// Dump a DB to disk.
			throw base::NotYetImplemented("flushing");
			for (auto& collection: sub_collections) collection->flush();
		}
	};
}
