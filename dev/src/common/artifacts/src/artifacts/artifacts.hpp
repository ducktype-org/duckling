#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>

#include <base/collections/optional.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>
#include <string_id/string_id.hpp>

#include <span>
#include <type_traits>
#include <vector>

namespace artifacts {
	/**
	 * @brief What a blob may hold is whatever the `ser` module can put on the wire: an
	 * aggregate needs no code at all, and anything else declares one of the `ser` hooks. A
	 * raw pointer is refused rather than copied, which is what the trivially-copyable
	 * requirement this replaced could not say.
	 *
	 * These two are the byte plumbing around `ser` - the buffer on the way out, the RawView
	 * on the way in - and nothing else.
	 *
	 *   out  a blob is written from an object we are already holding, so a failure is a bug
	 *        rather than a state to recover from - `ser::writeOrPanic` says so itself.
	 *   in   a blob is read back from a `.artc` file on disk, which is allowed to be
	 *        truncated, stale or damaged..
	 */
	template<class T>
	std::vector<byte> toBytes(const T& data) {
		std::vector<byte> bytes;
		::ser::writeOrPanic(bytes, data);
		return bytes;
	}

	/**
	 * @brief Constructs type `T` from the bytes of a blob, or nothing if they are not a `T`.
	 * @note Read as the unqualified `T`: a caller asking for `getData<decltype(SOME_CONST)>()`
	 * names a `const` type, and an object is built before it can be const - so the Optional
	 * carries the unqualified type too.
	 * @note The Optional costs one move of `T` and requires `T` to be movable, which the
	 * by-value read did not. That is the price of a damaged blob being an answer instead of a
	 * crash; a type that cannot be moved has to read through `ser` directly.
	 * @param view The blob's bytes.
	 * @return The object, or an empty Optional when the bytes do not decode.
	 */
	template<class T>
	base::Optional<std::remove_cv_t<T>> fromBytes(base::RawView view) {
		auto value = ::ser::read<std::remove_cv_t<T>>(std::span<const std::byte>{ view.getBegin(),
		                                                                          view.size() });
		if (!value) return {};
		return std::move(*value).take();
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
		template<class T>
		void setData(const T& data) {
			const std::vector<byte> bytes = toBytes(data);
			setData(bytes.data(), bytes.size());
		}

		/**
		 * @brief Gets blob's data.
		 * @note Data pointers can be invalidated by calls to `setData`.
		 */
		[[nodiscard]] base::RawView getDataView() const;

		/**
		 * @brief Gets blob's data and interprets them as a `T` object.
		 * @note Data pointers can be invalidated by calls to `setData`.
		 * @return The object, or an empty Optional when the blob does not decode as a `T` -
		 * a damaged or stale `.artc` on disk, which is a cache to drop rather than a crash.
		 */
		template<class T>
		base::Optional<std::remove_cv_t<T>> getData() const {
			return fromBytes<T>(getDataView());
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

		template<class T>
		void setBlobData(const BlobArtifact& blob, const T& data) {
			// lock will happen in the call bellow:
			const std::vector<byte> bytes = toBytes(data);
			setBlobData(blob, bytes.data(), bytes.size());
		}

		base::RawView getBlobDataView(const BlobArtifact& blob) const;

		/** @brief As BlobArtifact::getData, and empty for the same reasons. */
		template<class T>
		base::Optional<std::remove_cv_t<T>> getBlobData(const BlobArtifact& blob) const {
			// lock will happen in the call bellow:
			return fromBytes<T>(getBlobDataView(blob));
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

		template<class T>
		void setBlobDataNoLock(const BlobArtifact& blob, const T& data) {
			const std::vector<byte> bytes = toBytes(data);
			setBlobDataNoLock(blob, bytes.data(), bytes.size());
		}

		base::RawView getBlobDataViewNoLock(const BlobArtifact& blob) const;

		/** @brief As getBlobData, without taking the lock. */
		template<class T>
		base::Optional<std::remove_cv_t<T>> getBlobDataNoLock(const BlobArtifact& blob) const {
			return fromBytes<T>(getBlobDataViewNoLock(blob));
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
		 * @brief Writes the current `artifacts::BUILD_ID` to `<PATH>/.build_id`.
		 * This function should be called only on the root collection.
		 */
		void writeBuildIdFile();

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
