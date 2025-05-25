#pragma once

#include <filesystem/file.hpp>
#include <hashing/hash.hpp>

#include <base/box.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>
#include <base/string_id.hpp>

#include <type_traits>

namespace artifacts {
	template<class T>
	requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
	const T& interpret(base::RawView view) {
		CORE_ASSERT(view.size() == sizeof(T), "View\'s size does not match T\'s size");
		return *reinterpret_cast<T*>(view.getBegin());
	}

	/**
	 * @brief A collection of bytes.
	 * @note It cannot be read-only, because `std::vector<const byte>` does not compile.
	 */
	using Bytes = const std::vector<byte>;

	class ArtifactCollection;

	struct FileArtifact {
		const Ref<ArtifactCollection> PARENT;
		const base::StrID             NAME;

		/**
		 * @brief File that stores this `FileArtifact`'s data.
		 * @note Currently we are not providing any functionality regarding read/writing, so
		 * feel free to simple read and write to and from this file
		 * @note fs::FilePath when lazily reads the content for pointed file the first time, it will
		 * not reload it's content.
		 * @TODO Reviewer, do you think we should you std::filesystem::path for those?
		 */
		const fs::FilePath FILE;
	};

	struct BlobArtifact {
		const Ref<ArtifactCollection> PARENT;
		const base::StrID             NAME;

		/**
		 * @brief Sets blob's data. Invalidates current blob's data pointers.
		 */
		void setData(const byte* ptr, usize n_bytes) const;

		template<class T>
		requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
		void setData(const T& data) const {
			setData(&data, sizeof(data));
		}

		/**
		 * @brief Gets blob's data. Data pointers can be invalidated by calls to `setData`.
		 */
		[[nodiscard]] base::RawView getDataView() const;

		template<class T>
		requires std::is_standard_layout_v<T> && std::is_trivial_v<T> T& getData() {
			return interpret<T>(getDataView());
		}
	};

	/**
	 * @brief Represents an artifact group. (It basically maps to a directory.).
	 * All collections can be modified independently (because as of now there is no state hashing).
	 */
	class ArtifactCollection {
		static constexpr i64 PROTOCOL_VERSION = 1;

		base::HashMap<base::StrID, FileArtifact> file_artifacts;
		base::HashMap<base::StrID, BlobArtifact> blob_artifacts;
		base::HashMap<base::StrID, Box<Bytes>>   blob_data;

		base::HashMap<base::StrID, Box<ArtifactCollection>>
			sub_collections;  /// Box, because we may need stable refs.

	public:
		const fs::FilePath PATH;

		ArtifactCollection(const ArtifactCollection&)            = default;
		ArtifactCollection(ArtifactCollection&&)                 = default;
		ArtifactCollection& operator=(const ArtifactCollection&) = delete;
		ArtifactCollection& operator=(ArtifactCollection&&)      = delete;

		ArtifactCollection(fs::FilePath root);

		///////////////////////// GENERAL OPERATIONS ///////////////////////

		/**
		 * @brief Flushes ArtifactCollection's data to the disk.
		 * @note
		 */
		void flush();

		/////////////////////////// SUB COLLECTIONS /////////////////////////

		Ref<ArtifactCollection> subCollectionNew(base::StrID collection_name);

		Ref<ArtifactCollection> subCollectionAtOrNew(base::StrID collection_name);

		Ref<ArtifactCollection> subCollectionAt(base::StrID collection_name);

		base::Optional<Ref<ArtifactCollection>> subCollectionAtMaybe(base::StrID collection_name);

		/////////////////////////// FILE ARTIFACTS /////////////////////////

		const FileArtifact& fileArtifactNew(base::StrID artifact_name);

		const FileArtifact& fileArtifactAtOrNew(base::StrID artifact_name);

		const FileArtifact& fileArtifactAt(base::StrID artifact_name) const;

		base::Optional<const FileArtifact&> fileArtifactAtMaybe(base::StrID artifact_name) const;

		/////////////////////////// BLOB ARTIFACTS /////////////////////////

		const BlobArtifact& blobArtifactNew(base::StrID artifact_name);

		const BlobArtifact& blobArtifactAtOrNew(base::StrID artifact_name);

		const BlobArtifact& blobArtifactAt(base::StrID artifact_name) const;

		base::Optional<const BlobArtifact&> blobArtifactAtMaybe(base::StrID artifact_name) const;

		void setBlobData(BlobArtifact blob, const byte* ptr, usize n_bytes);

		template<class T>
		requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
		void setBlobData(BlobArtifact blob, const T& data) {
			setBlobData(blob, &data, sizeof(data));
		}

		base::RawView getBlobDataView(BlobArtifact blob) const;

		template<class T>
		requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
		const T& getBlobData(BlobArtifact blob) const {
			return interpret<T>(getBlobDataView(blob));
		}
	};
}
