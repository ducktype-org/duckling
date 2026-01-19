#pragma once

#include <base/collections/optional.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>

#include <cstring>
#include <type_traits>

namespace artifacts {
	/**
	 * @brief Represents a type, that is trivially interpretable as simple bytes.
	 * @note In the future this might be improved to feature custom serialization and
	 * deserialization as well.
	 */
	template<class T>
	concept SerdeType
		= std::is_standard_layout_v<T> && std::is_trivial_v<T> && std::is_trivially_copyable_v<T>;

	/**
	 * @brief Constructs type `T` from bytes.
	 */
	template<SerdeType T>
	T deserialize(base::RawView view) {
		CORE_ASSERT(view.size() == sizeof(T), "View\'s size does not match T\'s size");
		alignas(T) std::array<std::byte, sizeof(T)> buffer;
		std::memcpy(buffer.data(), view.getBegin(), sizeof(T));
		return *std::launder(reinterpret_cast<T*>(buffer.data()));
	}

	/**
	 * @brief A collection of bytes.
	 * @note `byte` cannot be read-only, because `std::vector<const byte>` does not compile.
	 */
	using Bytes = const std::vector<byte>;

	class ArtifactCollection;

	/**
	 * @brief Represents an artifact that maps to a file, e.g. an object file produced by the
	 * compiler.
	 *
	 * \parallel During compilation/lowering/backends files can be written to disk and concurrent
	 * builds of the same module/package can collide on paths. See:
	 *  - \ref dev/src/compiler/driver/driver/src/driver/operations/generic_operations.cpp "Driver
	 * operations"
	 *  - \ref
	 * dev/src/compiler/driver/driver/src_private/driver_private/backend_operations/compile_llvm.cpp
	 * "LLVM compilation"
	 *  - \ref
	 * dev/src/compiler/driver/driver/src_private/driver_private/backend_operations/compile_dvm.cpp"DVM
	 * compilation"
	 */
	struct FileArtifact final {
		const Ref<ArtifactCollection> PARENT;
		const base::StrID             NAME;

		/**
		 * @brief File that stores this `FileArtifact`'s data.
		 */
		const fs::File FILE;
	};

	/**
	 * @brief Represents an artifact, that can be represented as bytes, e.g. result of a query that
	 * returns an int.
	 */
	struct BlobArtifact final {
		const Ref<ArtifactCollection> PARENT;
		const base::StrID             NAME;

		/**
		 * @brief Sets blob's data.
		 * @note Invalidates current blob's data pointers.
		 */
		void setData(const byte* ptr, usize n_bytes);

		/**
		 * @brief Sets blob's data from serializable type `T`.
		 * @note Invalidates current blob's data pointers.
		 */
		template<SerdeType T>
		void setData(const T& data) {
			setData(reinterpret_cast<const byte*>(&data), sizeof(data));
		}

		/**
		 * @brief Gets blob's data.
		 * @note Data pointers can be invalidated by calls to `setData`.
		 */
		[[nodiscard]] base::RawView getDataView() const;

		/**
		 * @brief Gets blob's data and interprets them as a `T` object.
		 * @note Data pointers can be invalidated by calls to `setData`.
		 */
		template<SerdeType T>
		const T getData() const {
			return deserialize<T>(getDataView());
		}
	};

	/**
	 * @brief Represents an artifact group. (It basically maps to a directory.).
	 * All collections can be modified independently (because as of now there is no state hashing).
	 */
	class ArtifactCollection final {
	public:
		/**
		 * @brief Constructs ArtifactCollection, looks into `root` and restores previously saved
		 * `ArtifactCollection`s at `root` (if any).
		 */
		ArtifactCollection(std::filesystem::path root);

		ArtifactCollection(const ArtifactCollection&)            = delete;
		ArtifactCollection(ArtifactCollection&&)                 = delete;
		ArtifactCollection& operator=(const ArtifactCollection&) = delete;
		ArtifactCollection& operator=(ArtifactCollection&&)      = delete;

		///////////////////////// GENERAL OPERATIONS ///////////////////////

		/**
		 * @brief Flushes ArtifactCollection tree data to the disk.
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

		base::Optional<base::CRef<FileArtifact>> fileArtifactAtMaybe(base::StrID artifact_name
		) const;

		/////////////////////////// BLOB ARTIFACTS /////////////////////////

		const BlobArtifact& blobArtifactNew(base::StrID artifact_name);

		const BlobArtifact& blobArtifactAtOrNew(base::StrID artifact_name);

		const BlobArtifact& blobArtifactAt(base::StrID artifact_name) const;

		base::Optional<base::CRef<BlobArtifact>> blobArtifactAtMaybe(base::StrID artifact_name
		) const;

		void setBlobData(const BlobArtifact& blob, const byte* ptr, usize n_bytes);

		template<SerdeType T>
		void setBlobData(const BlobArtifact& blob, const T& data) {
			setBlobData(blob, reinterpret_cast<const byte*>(&data), sizeof(data));
		}

		base::RawView getBlobDataView(const BlobArtifact& blob) const;

		template<SerdeType T>
		const T getBlobData(const BlobArtifact& blob) const {
			return deserialize<T>(getBlobDataView(blob));
		}

		/////////////////////////// PRIVATE /////////////////////////

	private:
		const std::filesystem::path PATH;

		/**
		 * @name  Artifacts storage
		 * @brief Global artifacts hierarchy for build/query outputs (files and blobs), persisted to
		 * disk.
		 * \parallel Written by CompileModule and other driver operations; concurrent writes can race.
		 * @note Accessed by \ref getRootCollection and \ref setRootCollection
		 * @{
		 */
		base::HashMap<base::StrID, FileArtifact> file_artifacts;
		base::HashMap<base::StrID, BlobArtifact> blob_artifacts;
		base::HashMap<base::StrID, Box<Bytes>>   blob_data;

		/**
		 * Box, because we may need stable refs. Cannot be base::StableHashMap, because we are using
		 * a private constructor of collection.
		 */
		base::HashMap<base::StrID, Box<ArtifactCollection>> sub_collections;
		/**
		 * @}
		 */

		const base::Optional<Ref<ArtifactCollection>> PARENT;

		/**
		 * @brief Implementation that writes the blob data on the disk and propagates down the
		 * ArtifactCollection tree.
		 * @note It is a helper method for `flush()`.
		 */
		void flushDown();

		/**
		 * @brief Construct a new ArtifactCollection and sets the parent variable.
		 */
		ArtifactCollection(std::filesystem::path path, Ref<ArtifactCollection> parent);

		/**
		 * @brief Parses blobs from `content` and inserts them to the collection.
		 * @note It is a helper method for `loadData()`.
		 */
		void parseBlobsFromBytes(std::stringstream& content);

		/**
		 * @brief Iterates over `PATH` files and directories, attaches artifacts and sub-collections.
		 */
		void loadData();

		/**
		 * @brief Return path to a `.artc` file with blob content.
		 * .artc file is the file which stores blob data.
		 */
		fs::FilePath getArtcFile() const;
	};
}
