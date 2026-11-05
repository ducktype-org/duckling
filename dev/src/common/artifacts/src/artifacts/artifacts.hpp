// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>

#include <base/collections/optional.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ok_bad.hpp>

#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>

#include <chrono>
#include <cstring>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

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
		 * @brief Name of the file, kept in every collection's directory, that lists the file
		 * artifacts and sub-collections this collection created there.
		 *
		 * The artifacts folder is chosen by the user and may well be a folder with their own
		 * files in it (`-a .`), so the cache never assumes that what it finds there is its own.
		 * Only what this list names is ever loaded as an artifact or removed when the cache is
		 * cleared; everything else is left alone. The list cannot be replaced by a marker inside
		 * each artifact, because most artifacts are files whose format is not ours to extend:
		 * executables, archives, object files, LLVM IR.
		 *
		 * An entry is never owned if it leads out of the directory (absolute, `..`), back to the
		 * directory itself (`.`), or through a symlink at any part of its path. The compiler
		 * creates none of those, so such an entry is skipped, like the module tree builder
		 * skips symlinks among source files.
		 *
		 * Format: the header line `duckling-artifacts 1`, then one entry per line - `f <name>`
		 * for a file artifact, `c <name>` for a sub-collection.
		 */
		static constexpr std::string_view OWNED_LIST_FILE = ".duck_artifacts";

		/**
		 * @brief Constructs ArtifactCollection, looks into `root` and restores previously saved
		 * `ArtifactCollection`s at `root` (if any).
		 *
		 * If the build id stored at `root/.build_id` is missing or does not match the current
		 * compiler's build id, the artifacts the cache owns there are removed before loading
		 * (controlled by the compile-time flag `artifacts::CHECK_BUILD_ID`).
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

		/// What `removeCache` did.
		enum class RemoveCacheResult {
			/// Everything the cache owned is gone.
			Removed,
			/// `root` is missing, or holds nothing of the cache's (no `OWNED_LIST_FILE`).
			NothingToRemove,
			/// `root` is not a folder.
			NotAFolder,
			/// Something owned could not be removed; that was reported to the user, and the
			/// rest was removed anyway.
			Incomplete,
		};

		/**
		 * @brief Removes the whole cache at `root`: every artifact and sub-collection it owns
		 * (see `OWNED_LIST_FILE`), and its own bookkeeping files. Nothing else in `root` is
		 * touched, so pointing it at a folder of the user's own files is safe. Meant for an
		 * explicit request such as `duckc clean`; a build using `root` at the same time loses
		 * its cache and can fail.
		 */
		static RemoveCacheResult removeCache(const std::filesystem::path& root);

		/**
		 * @brief The sentence `duckc clean` prints for a `removeCache` result.
		 * @param root The folder as the user gave it.
		 */
		[[nodiscard]]
		static std::string describeRemoveCache(RemoveCacheResult result, std::string_view root);

		/// Whether a `removeCache` result is a failure, i.e. `duckc clean` exits with an error.
		[[nodiscard]]
		static bool isRemoveCacheFailure(RemoveCacheResult result);

		/**
		 * @brief Removes the file artifacts of this collection and of its sub-collections that
		 * have not been written or looked up (see `fileArtifactAtMaybe`) for `max_age`.
		 *
		 * This is how a cache shared by several builds is kept from growing forever. Every build
		 * marks what it uses, so whatever any build still needs is fresh, and only what none of
		 * them has touched for `max_age` is removed - no build has to know what the others are
		 * building. `max_age` should therefore be far longer than any build takes.
		 *
		 * @return How many file artifacts were removed.
		 */
		usize removeFileArtifactsUnusedFor(std::chrono::seconds max_age);

		/**
		 * @brief Removes the temporary files, older than `max_age`, that killed writes left of
		 * this collection's files and of its sub-collections' files.
		 *
		 * Every file is written to `<name>.<random>.tmp` next to it and renamed into place (see
		 * `fs::FileManager::produceFileAtomically`). A process killed meanwhile - e.g. Ctrl+C
		 * during linking - leaves that file behind, and nothing else would ever remove it.
		 * `max_age` keeps the files of writes still in progress in other builds sharing the
		 * folder, so it should be far longer than any single write takes.
		 *
		 * @return How many temporary files were removed.
		 */
		usize removeTemporaryFilesOlderThan(std::chrono::seconds max_age);

		/////////////////////////// SUB COLLECTIONS /////////////////////////

		Ref<ArtifactCollection> subCollectionNew(base::StrID collection_name);

		Ref<ArtifactCollection> subCollectionAtOrNew(base::StrID collection_name);

		Ref<ArtifactCollection> subCollectionAt(base::StrID collection_name);

		base::Optional<Ref<ArtifactCollection>> subCollectionAtMaybe(base::StrID collection_name);

		/////////////////////////// FILE ARTIFACTS /////////////////////////

		FileArtifact fileArtifactNew(base::StrID artifact_name);

		/*
		 * Every lookup below that hands out an existing file artifact also marks it as used:
		 * it sets the file's modification time to now. That is what keeps the artifact from
		 * being removed by `removeFileArtifactsUnusedFor`, in this build or in any other build
		 * sharing the folder - so there is no way to use an artifact without marking it. A file
		 * that cannot be marked (e.g. on a read-only mount) is still handed out.
		 */

		/**
		 * @brief Returns the file artifact `artifact_name`, marked as used, or creates it with
		 * `fileArtifactNew` if it is not there - or if its file is gone.
		 */
		FileArtifact fileArtifactAtOrNew(base::StrID artifact_name);

		/**
		 * @brief Returns the file artifact `artifact_name`, marked as used. It must be there.
		 */
		FileArtifact fileArtifactAt(base::StrID artifact_name);

		/**
		 * @brief Looks up a file artifact, e.g. a previous build's output that is still up to
		 * date, and marks it as used.
		 *
		 * @return The artifact, or nothing if it is not in the collection or its file is gone
		 * (e.g. removed by another build). Either way, the caller has a cache miss.
		 */
		base::Optional<FileArtifact> fileArtifactAtMaybe(base::StrID artifact_name);

		/**
		 * @brief Removes a file artifact and its file.
		 * @return True if it was removed. A file that cannot be removed is reported to the user,
		 * and the artifact is then kept.
		 */
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

		/**
		 * @brief The marking lookup behind the public `fileArtifactAt*` methods; see
		 * `fileArtifactAtMaybe`.
		 */
		base::Optional<FileArtifact> fileArtifactInUseNoLock(base::StrID artifact_name);

		/// Plain lookup, which does not mark the artifact as used - for internal use only.
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

		/**
		 * @brief Names already recorded in this directory's `OWNED_LIST_FILE`, so that an entry
		 * is appended to it only once.
		 */
		std::unordered_set<std::string> listed_names;

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
		 * @brief Writes this collection's blob cache to its `.artc` file with
		 * `fs::FileManager::writeFileAtomically`; a failure is reported, not fatal.
		 *
		 * That matters more than it looks: a torn `.artc` sitting next to a *matching*
		 * `.build_id` is not cleared by the build-id check, so it used to break every later build
		 * in that directory until someone deleted the folder by hand.
		 */
		void writeArtcFile() const;

		/**
		 * @brief Rewrites this collection's `OWNED_LIST_FILE`, atomically, so that it names
		 * exactly the owned entries that are still on disk.
		 *
		 * Entries whose file is gone (e.g. through `deleteFileArtifact`) are dropped, so the list
		 * does not grow without bound. Entries other processes appended are kept, unless one is
		 * appended between this rewrite's read and its rename. Its owner restores it on its own
		 * flush; only if that process is killed first does the entry stay lost. Its file is then
		 * left on disk unowned: never removed by mistake, and recorded again the next time it is
		 * created.
		 */
		void writeOwnedList();

		/**
		 * @brief Records `name` in `OWNED_LIST_FILE`, unless it is already there.
		 *
		 * Called before the entry is created on disk, so that even a build killed right after
		 * leaves nothing behind that the next cleanup would not know about.
		 *
		 * @param kind `'f'` for a file artifact, `'c'` for a sub-collection.
		 */
		void recordOwnedEntry(char kind, std::string_view name);

		/**
		 * @brief Construct a new ArtifactCollection and sets the parent variable.
		 */
		ArtifactCollection(std::filesystem::path path, Ref<ArtifactCollection> parent);

		/**
		 * @brief If the stored build id at `<PATH>/.build_id` differs from the current
		 * compiler's `artifacts::BUILD_ID`, or the file is missing entirely, removes the
		 * artifacts the cache owns in `PATH` (see `OWNED_LIST_FILE`) and, if there were any,
		 * logs a user-visible message.
		 */
		void validateOrWipeBuildId();

		/**
		 * @brief Removes what the cache owns in `PATH` - the entries `OWNED_LIST_FILE` names,
		 * recursively, and the cache's own bookkeeping files - and forgets it in memory, so the
		 * build carries on from an empty cache. Files the list does not name are left alone.
		 *
		 * Entries it fails to remove are reported as warnings and skipped - a cache that cannot
		 * be cleared is not worth aborting over.
		 */
		void discardOwnedContents();

		/**
		 * @brief Gives up on the cache in `PATH`: tells the user why and discards it with
		 * `discardOwnedContents`.
		 *
		 * @param what_is_wrong Fills in "The artifacts cache at '<path>' <what_is_wrong>.", e.g.
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
		 * is reported.
		 *
		 * @note It is a helper method for `loadData()`.
		 * @return `base::BAD` if `content` is not a whole, well-formed `.artc` body. Each blob
		 * is added to the collection as soon as it is read, so on failure the blobs that came
		 * before the bad field are already in it; the caller discards them (see
		 * `discardUnusableCache`).
		 */
		base::OkBad parseBlobsFromBytes(std::stringstream& content);

		/**
		 * @brief Restores the blobs from the `.artc` file, and attaches the file artifacts and
		 * sub-collections that `OWNED_LIST_FILE` names and that are still on disk. Nothing else
		 * in `PATH` is attached.
		 */
		void loadData();

		/**
		 * @brief Return path to a `.artc` file with blob content.
		 * .artc file is the file which stores blob data.
		 */
		fs::FilePath getArtcFile() const;
	};
}
