#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>

#include <base/collections/optional.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ok_bad.hpp>

#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>

#include <cstring>
#include <sstream>
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
	 * @note This type is intentially simple and copyable, think of it as a File-ID.
	 *
	 * \parallel There is at the moment no synchronization on file access. During
	 * compilation/lowering/backends files can be written to disk. Processed files and backend
	 * outputs (LLVM IR/ASM/object files, DVM files) in concurrent builds of the same module/package
	 * can collide on paths. See:
	 *  - \ref dev/src/compiler/driver/driver/src/driver/operations/generic_operations.cpp
	 *  - \ref
	 * dev/src/compiler/driver/driver/src_private/driver_private/backend_operations/compile_llvm.cpp
	 *  - \ref
	 * dev/src/compiler/driver/driver/src_private/driver_private/backend_operations/compile_dvm.cpp
	 */
	struct FileArtifact final {
		const Ref<ArtifactCollection> parent;
		const base::StrID             name;

		/**
		 * @brief File that stores this `FileArtifact`'s data.
		 */
		const fs::File file;
	};

	/**
	 * @brief Represents an artifact, that can be represented as bytes, e.g. result of a query that
	 * returns an int.
	 *
	 * @note This type is intentially simple and copyable, think of it as a Blob-ID.
	 *
	 * @note Methods call on this object are thread safe, but there is no synchronization beyond
	 * that. If threads are writing to the blob, while someone is reading content by using the view
	 * from `getDataView`, it will result in a race condition.
	 */
	struct BlobArtifact final {
		const Ref<ArtifactCollection> parent;
		const base::StrID             name;

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
		 * @brief Name of the file (stored inside the root collection's directory) that holds the
		 * compiler build id that produced the artifacts.
		 */
		static constexpr std::string_view BUILD_ID_FILE = ".build_id";

		/**
		 * @brief Constructs ArtifactCollection, looks into `root` and restores previously saved
		 * `ArtifactCollection`s at `root` (if any).
		 *
		 * If a stored build id exists at `root/.build_id` and it does not match the current
		 * compiler's build id, the directory contents are wiped before loading (controlled by
		 * the compile-time flag `artifacts::CHECK_BUILD_ID`).
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

		FileArtifact fileArtifactNew(base::StrID artifact_name);

		FileArtifact fileArtifactAtOrNew(base::StrID artifact_name);

		FileArtifact fileArtifactAt(base::StrID artifact_name) const;

		base::Optional<base::CRef<FileArtifact>> fileArtifactAtMaybe(base::StrID artifact_name
		) const;

		bool deleteFileArtifact(base::StrID artifact_name);

		/////////////////////////// BLOB ARTIFACTS /////////////////////////

		BlobArtifact blobArtifactNew(base::StrID artifact_name);

		BlobArtifact blobArtifactAtOrNew(base::StrID artifact_name);

		BlobArtifact blobArtifactAt(base::StrID artifact_name) const;

		base::Optional<base::CRef<BlobArtifact>> blobArtifactAtMaybe(base::StrID artifact_name
		) const;

		void setBlobData(const BlobArtifact& blob, const byte* ptr, usize n_bytes);

		template<SerdeType T>
		void setBlobData(const BlobArtifact& blob, const T& data) {
			// lock will happen in the call bellow:
			setBlobData(blob, reinterpret_cast<const byte*>(&data), sizeof(data));
		}

		base::RawView getBlobDataView(const BlobArtifact& blob) const;

		template<SerdeType T>
		const T getBlobData(const BlobArtifact& blob) const {
			// lock will happen in the call bellow:
			return deserialize<T>(getBlobDataView(blob));
		}

		/////////////////////////// PRIVATE /////////////////////////

	private:
		/////////////////// NO LOCK INTERNALL API ///////////////////

		/**
		 * @brief Flushes ArtifactCollection tree data to the disk.
		 */
		void flushNoLock();

		Ref<ArtifactCollection> subCollectionNewNoLock(base::StrID collection_name);

		Ref<ArtifactCollection> subCollectionAtOrNewNoLock(base::StrID collection_name);

		Ref<ArtifactCollection> subCollectionAtNoLock(base::StrID collection_name);

		base::Optional<Ref<ArtifactCollection>> subCollectionAtMaybeNoLock(base::StrID collection_name
		);

		FileArtifact fileArtifactNewNoLock(base::StrID artifact_name);

		FileArtifact fileArtifactAtOrNewNoLock(base::StrID artifact_name);

		FileArtifact fileArtifactAtNoLock(base::StrID artifact_name) const;

		base::Optional<base::CRef<FileArtifact>> fileArtifactAtMaybeNoLock(base::StrID artifact_name
		) const;

		bool deleteFileArtifactNoLock(base::StrID artifact_name);

		BlobArtifact blobArtifactNewNoLock(base::StrID artifact_name);

		BlobArtifact blobArtifactAtOrNewNoLock(base::StrID artifact_name);

		BlobArtifact blobArtifactAtNoLock(base::StrID artifact_name) const;

		base::Optional<base::CRef<BlobArtifact>> blobArtifactAtMaybeNoLock(base::StrID artifact_name
		) const;

		void setBlobDataNoLock(const BlobArtifact& blob, const byte* ptr, usize n_bytes);

		template<SerdeType T>
		void setBlobDataNoLock(const BlobArtifact& blob, const T& data) {
			setBlobDataNoLock(blob, reinterpret_cast<const byte*>(&data), sizeof(data));
		}

		base::RawView getBlobDataViewNoLock(const BlobArtifact& blob) const;

		template<SerdeType T>
		const T getBlobDataNoLock(const BlobArtifact& blob) const {
			return deserialize<T>(getBlobDataViewNoLock(blob));
		}

		///////////////////////// OBJECT STATE //////////////////////


		const std::filesystem::path PATH;

		mutable concurrent::AtomicFlagSpinlock lock;

		/**
		 * @brief RAII struct for locking the collection.
		 */
		struct WithLock final {
			const ArtifactCollection& collection;

			WithLock(const ArtifactCollection& collection): collection(collection) {
				collection.lock.lock();
			}

			~WithLock() { collection.lock.unlock(); }
		};

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
		 * True if the build_id was not present or was invalid
		 * and we need to create or override it, false if it was valid.
		 */
		bool flush_build_id = true;

		/**
		 * @brief Implementation that writes the blob data on the disk and propagates down the
		 * ArtifactCollection tree.
		 * @note It is a helper method for `flush()`.
		 */
		void flushDown();

		/**
		 * @brief Writes this collection's blob cache to its `.artc` file, atomically.
		 *
		 * Goes through a sibling temporary file that is renamed into place, so a write that
		 * fails or dies partway leaves the previous `.artc` intact rather than a torn one. A
		 * failure is reported to the user and otherwise ignored - see
		 * `reportCacheWriteFailure`.
		 */
		void writeArtcFile() const;

		/**
		 * @brief Tells the user that the artifacts cache could not be saved, and why.
		 *
		 * A failure here is not fatal: everything this build produced is already on disk, so
		 * only the next build's incremental reuse is lost.
		 *
		 * @param path The cache file that could not be written.
		 * @param reason Human-readable cause, e.g. an `std::error_code` message.
		 */
		static void reportCacheWriteFailure(const fs::FilePath& path, std::string_view reason);

		/**
		 * @brief Construct a new ArtifactCollection and sets the parent variable.
		 */
		ArtifactCollection(std::filesystem::path path, Ref<ArtifactCollection> parent);

		/**
		 * @brief If the stored build id at `<PATH>/.build_id` differs from the current
		 * compiler's `artifacts::BUILD_ID`, or the file is missing entirely, wipes
		 * `PATH`'s contents (but not `PATH` itself) and logs a user-visible message.
		 */
		void validateOrWipeBuildId();

		/**
		 * @brief Removes everything inside `PATH`, leaving `PATH` itself, so that the next
		 * build starts from an empty cache. Entries it fails to remove are reported as
		 * warnings and skipped - a cache that cannot be cleared is not worth aborting over.
		 */
		void clearCacheFolder();

		/**
		 * @brief Gives up on the cache in `PATH`: tells the user why, drops the blobs loaded so
		 * far and clears the folder, so the build carries on from an empty cache.
		 *
		 * @param what_is_wrong Fills in "Artifacts at '<path>' <what_is_wrong>.", e.g.
		 * "is corrupted".
		 */
		void discardUnusableCache(std::string_view what_is_wrong);

		/**
		 * @brief Writes the current `artifacts::BUILD_ID` to `<PATH>/.build_id`.
		 * This function should be called only on the root collection.
		 */
		void writeBuildIdFile();

		/**
		 * @brief Parses blobs from `content` and inserts them to the collection.
		 *
		 * A `.artc` can be torn in half by a write that ran out of disk or a build that was
		 * killed, and its tail then decodes into nonsense. That is a reason to drop the cache,
		 * never to bring the compiler down, so every field is validated and a malformed file
		 * is reported rather than asserted on.
		 *
		 * @note It is a helper method for `loadData()`.
		 * @return `base::BAD` if `content` is not a whole, well-formed `.artc` body. Blobs
		 * parsed before the failure may already be in the collection; the caller is expected
		 * to discard them.
		 */
		base::OkBad parseBlobsFromBytes(std::stringstream& content);

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
