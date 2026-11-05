// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "artc_format.hpp"
#include "cache_cleanup.hpp"
#include "cache_files.hpp"
#include "owned_list.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <artifacts/artifacts.hpp>
#include <artifacts/build_id.hpp>
#include <artifacts/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
#include <logger/logger.hpp>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace artifacts::detail;

void artifacts::BlobArtifact::setData(const byte* ptr, usize n_bytes) {
	parent->setBlobData(*this, ptr, n_bytes);
}

base::RawView artifacts::BlobArtifact::getDataView() const {
	return parent->getBlobDataView(*this);
}

//////////// COLLECTION EXTERNALLY VISIBLE METHODS ////////////

artifacts::ArtifactCollection::ArtifactCollection(fs::FilePath root): folder(std::move(root)) {
	CORE_ASSERT(folder.isDirectory(), "ArtifactCollection path is not an existing directory");
	if constexpr (CHECK_BUILD_ID) validateOrWipeBuildId();
	loadData();
}

void artifacts::ArtifactCollection::flush() {
	WithLock lock(*this);
	this->flushNoLock();
}

artifacts::ArtifactCollection::RemoveCacheResult artifacts::ArtifactCollection::removeCache(
	const fs::FilePath& root
) {
	if (!root.exists()) return RemoveCacheResult::NothingToRemove;
	if (!root.isDirectory()) return RemoveCacheResult::NotAFolder;
	// Without the list nothing in the folder is the cache's, so nothing is touched at all.
	if (!cacheFilePath(root, CacheFile::OwnedList).exists())
		return RemoveCacheResult::NothingToRemove;
	return removeOwnedContents(root) ? RemoveCacheResult::Removed : RemoveCacheResult::Incomplete;
}

std::string artifacts::ArtifactCollection::describeRemoveCache(
	RemoveCacheResult result, std::string_view root
) {
	const auto quoted = base::strConcat("\"", root, "\"");
	switch (result) {
	case RemoveCacheResult::Removed:
		return base::strConcat("Removed the build artifacts in ", quoted, ".");
	case RemoveCacheResult::NothingToRemove:
		return base::strConcat("Nothing to clean in ", quoted, ".");
	case RemoveCacheResult::NotAFolder:
		return base::strConcat("Error: ", quoted, " is not a folder.");
	case RemoveCacheResult::Incomplete:
		return base::strConcat(
			"Error: some artifacts in ", quoted, " could not be removed; see the warnings above."
		);
	}
	CORE_UNREACHABLE();
}

bool artifacts::ArtifactCollection::isRemoveCacheFailure(RemoveCacheResult result) {
	return result == RemoveCacheResult::NotAFolder || result == RemoveCacheResult::Incomplete;
}

usize artifacts::ArtifactCollection::removeFileArtifactsUnusedFor(timer::Duration max_age) {
	WithLock lock(*this);

	std::vector<base::StrID> unused;
	for (const auto& [name, artifact]: file_artifacts) {
		const auto age = artifact.path.timeSinceModified();
		if (age.has_value() && age.value() > max_age) unused.push_back(name);
	}

	usize removed = 0;
	for (const auto& name: unused) removed += deleteFileArtifactNoLock(name) ? 1 : 0;
	for (auto&& [_, sub_collection]: sub_collections)
		removed += sub_collection->removeFileArtifactsUnusedFor(max_age);
	return removed;
}

usize artifacts::ArtifactCollection::removeTemporaryFilesOlderThan(timer::Duration max_age) {
	WithLock lock(*this);

	std::vector<std::string> written_names = cacheFileNames(folder);
	for (const auto& [name, _]: file_artifacts) written_names.emplace_back(name.strView());

	usize removed = 0;
	for (const auto& temp_file: findTempFiles(folder, written_names)) {
		const auto age = temp_file.timeSinceModified();
		if (age.has_value() && age.value() > max_age && removeOrWarn(temp_file)) removed++;
	}
	for (auto&& [_, sub_collection]: sub_collections)
		removed += sub_collection->removeTemporaryFilesOlderThan(max_age);
	return removed;
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAtOrNew(
	base::StrID collection_name
) {
	WithLock lock(*this);
	if (auto existing = sub_collections.atMaybe(collection_name); existing.has_value())
		return existing.value()->refMut();
	return subCollectionNewNoLock(collection_name);
}

base::Optional<Ref<artifacts::ArtifactCollection>> artifacts::ArtifactCollection::subCollectionAtMaybe(
	base::StrID collection_name
) {
	WithLock lock(*this);
	return sub_collections.atMaybe(collection_name).map([](const auto& box) {
		return box->refMut();
	});
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactAtOrNew(base::StrID artifact_name
) {
	WithLock lock(*this);
	// For writing: a file another build removed meanwhile is simply written again.
	if (auto existing = file_artifacts.atMaybe(artifact_name); existing.has_value()) {
		std::ignore = fs::FileManager::touch(existing.value()->path);
		return *existing.value();
	}
	return fileArtifactNewNoLock(artifact_name);
}

base::Optional<artifacts::FileArtifact> artifacts::ArtifactCollection::fileArtifactAtMaybe(
	base::StrID artifact_name
) {
	WithLock lock(*this);
	return findAndMarkUsedNoLock(artifact_name);
}

bool artifacts::ArtifactCollection::deleteFileArtifact(base::StrID artifact_name) {
	WithLock lock(*this);
	return deleteFileArtifactNoLock(artifact_name);
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactAtOrNew(base::StrID artifact_name
) {
	WithLock lock(*this);
	if (auto existing = blob_artifacts.atMaybe(artifact_name); existing.has_value())
		return *existing.value();
	return blobArtifactNewNoLock(artifact_name);
}

base::Optional<artifacts::BlobArtifact> artifacts::ArtifactCollection::blobArtifactAtMaybe(
	base::StrID artifact_name
) const {
	WithLock lock(*this);
	return blob_artifacts.atMaybe(artifact_name).map([](const auto& blob) { return *blob; });
}

void artifacts::ArtifactCollection::setBlobData(
	const BlobArtifact& blob, const byte* ptr, usize n_bytes
) {
	WithLock lock(*this);
	CORE_ASSERT(blob.parent.get() == this, "Blob does not belong to this collection");
	blob_data[blob.name] = makeBox<Bytes>(ptr, ptr + n_bytes);
}

base::RawView artifacts::ArtifactCollection::getBlobDataView(const BlobArtifact& blob) const {
	WithLock lock(*this);
	CORE_ASSERT(blob.parent.get() == this, "Blob does not belong to this collection");
	const auto& data = blob_data[blob.name];
	return { data->data(), data->size() };
}

/////////////////////// LOADING AND DISCARDING //////////////////////

artifacts::ArtifactCollection::ArtifactCollection(
	fs::FilePath path, Ref<ArtifactCollection> parent_collection
):
	  folder(std::move(path)),
	  parent(parent_collection) {
	loadData();
}

void artifacts::ArtifactCollection::validateOrWipeBuildId() {
	const auto  build_id_path = cacheFilePath(folder, CacheFile::BuildId);
	std::string stored_id;
	const bool  has_file = build_id_path.exists();
	if (has_file) {
		// An unreadable build id is as good as a wrong one: the cache cannot be trusted.
		const auto content = fs::File::open(build_id_path).and_then([](const fs::File& file) {
			return file.getContent();
		});
		if (content.has_value()) stored_id = content->view().stdString();
	}

	if (has_file && stored_id == BUILD_ID) {
		flush_build_id = false;
		return;
	}

	if (has_file) {
		CORE_USER_LOG(
			"The artifacts cache at '",
			folder.string(),
			"' was produced by a different build of the compiler (stored build id: ",
			stored_id.empty() ? std::string("<empty>") : stored_id.substr(0, 12) + "...",
			", current build id: ",
			std::string(BUILD_ID).substr(0, 12),
			"...). Removing the cached artifacts and starting fresh.\n"
		);
	} else if (cacheFilePath(folder, CacheFile::OwnedList).exists()) {
		CORE_USER_LOG(
			"The artifacts cache at '",
			folder.string(),
			"' has no build id. Removing the cached artifacts and starting fresh.\n"
		);
	}

	discardOwnedContents();
}

void artifacts::ArtifactCollection::loadData() {
	// Without a list this folder is not a collection of ours, so whatever is in it - an `.artc`
	// included - is not ours to load. Flushing writes this collection's own `.artc` over it.
	const auto owned = readOwnedList(folder);
	if (!owned.has_value()) return;
	for (const auto& entry: owned.value()) listed_names.insert(entry.name);

	const auto artc_path = cacheFilePath(folder, CacheFile::Artc);
	if (artc_path.exists()) {
		const auto content = fs::File::open(artc_path).and_then([](const fs::File& file) {
			return file.getContent();
		});
		if (!content.has_value()) {
			discardUnusableCache("cannot be read");
			return;
		}
		const auto blobs = parseArtc(content->view().stringView());
		if (!blobs.has_value()) {
			discardUnusableCache("is corrupted");
			return;
		}
		for (const auto& stored: blobs.value()) {
			const auto blob      = blobArtifactNewNoLock(base::StrID(stored.name));
			blob_data[blob.name] = makeBox<Bytes>(stored.data);
		}
	}

	// Attach the file artifacts and sub-collections. Only what the list names is ours: anything
	// else in the folder belongs to the user or to another tool.
	for (const auto& entry: owned.value()) {
		if (!isOwnable(folder, entry.name)) continue;
		const auto path = entryPath(entry.name);
		const auto name = base::StrID(entry.name);
		if (entry.kind == OWNED_COLLECTION && path.isDirectory())
			subCollectionNewNoLock(name);
		else if (entry.kind == OWNED_FILE && path.isRegularFile())
			fileArtifactNewNoLock(name);
	}
}

void artifacts::ArtifactCollection::discardUnusableCache(std::string_view what_is_wrong) {
	CORE_USER_LOG(
		"The artifacts cache at '",
		folder.string(),
		"' ",
		what_is_wrong,
		". Removing the cached artifacts and starting fresh.\n"
	);
	discardOwnedContents();
}

void artifacts::ArtifactCollection::discardOwnedContents() {
	removeOwnedContents(folder);
	// Whatever was loaded so far describes a cache that is now gone, so it must not stay in
	// memory - it would be flushed straight back onto disk at exit.
	blob_artifacts.clear();
	blob_data.clear();
	file_artifacts.clear();
	sub_collections.clear();
	listed_names.clear();
	// The build id went with the rest; without this, a build that found a matching one would
	// not write it back, and the next build would discard the fresh cache all over again.
	flush_build_id = true;
}

/////////////////////////////// FLUSHING //////////////////////////////

void artifacts::ArtifactCollection::flushNoLock() {
	if (parent) {
		parent.value()->flush();
	} else {
		flushDown();
		if constexpr (CHECK_BUILD_ID) writeBuildIdFile();
	}
}

void artifacts::ArtifactCollection::flushDown() {
	writeArtcFile();
	writeOwnedList();

	// Every sub-collection gets its turn whatever happened above: one directory we cannot write
	// to must not cost the whole tree its cache.
	for (auto&& [_, sub_collection]: sub_collections) sub_collection->flushDown();
}

void artifacts::ArtifactCollection::writeArtcFile() const {
	CORE_DEV_LOG(Artifacts, "Flushing ArtifactCollection at: ", folder.absolute().string(), "\n");

	std::vector<std::pair<std::string_view, base::RawView>> blobs;
	blobs.reserve(blob_artifacts.size());
	for (const auto& [name, _]: blob_artifacts) {
		const auto& data = blob_data[name];
		blobs.emplace_back(name.strView(), base::RawView{ data->data(), data->size() });
	}
	writeCacheFile(folder, CacheFile::Artc, [&blobs](std::ostream& out) { writeArtc(out, blobs); });
}

void artifacts::ArtifactCollection::writeOwnedList() {
	std::vector<OwnedEntry> candidates;
	for (const auto& [name, _]: file_artifacts)
		candidates.push_back({ .kind = OWNED_FILE, .name = std::string(name.strView()) });
	for (const auto& [name, _]: sub_collections)
		candidates.push_back({ .kind = OWNED_COLLECTION, .name = std::string(name.strView()) });
	// Another process sharing this folder may have created entries this one never loaded.
	for (auto& entry: readOwnedList(folder).copyValueOrDefault())
		candidates.push_back(std::move(entry));

	// A file whose write was killed has only its temporary file; the entry has to stay, or
	// nothing would own - and ever remove - that leftover.
	std::vector<std::string> file_names;
	for (const auto& entry: candidates)
		if (entry.kind == OWNED_FILE) file_names.push_back(entry.name);
	std::unordered_set<std::string> with_leftovers;
	for (const auto& temp_file: findTempFiles(folder, file_names)) {
		if_opt_some(fs::FileManager::temporaryFileTarget(temp_file), target) {
			with_leftovers.insert(target.lexicallyNormal().string());
		}
	}

	std::unordered_set<std::string> kept;
	std::vector<OwnedEntry>         entries;
	for (auto& entry: candidates) {
		if (kept.contains(entry.name) || !isOwnable(folder, entry.name)) continue;
		// An entry whose file is gone (e.g. through `deleteFileArtifact`, or never written) has
		// nothing left to own, so it is dropped here instead of piling up.
		const auto path = entryPath(entry.name);
		if (!path.exists() && !with_leftovers.contains(path.lexicallyNormal().string())) continue;
		kept.insert(entry.name);
		entries.push_back(std::move(entry));
	}

	const auto content = formatOwnedList(entries);
	writeCacheFile(folder, CacheFile::OwnedList, [&content](std::ostream& out) { out << content; });
	listed_names = std::move(kept);
}

void artifacts::ArtifactCollection::writeBuildIdFile() {
	if (!flush_build_id) return;
	// Written directly rather than as a file artifact: it is the cache's own bookkeeping, like
	// the `.artc`, so it does not belong in the owned list.
	//
	// Losing it is self-correcting rather than dangerous: the next build finds no build id, says
	// so and removes the cached artifacts, which costs one full rebuild. So a failure only warns
	// and lets this build finish.
	writeCacheFile(folder, CacheFile::BuildId, [](std::ostream& out) { out << BUILD_ID; });
}

void artifacts::ArtifactCollection::recordOwnedEntry(char kind, std::string_view name) {
	if (listed_names.contains(std::string(name)) || !isOwnable(folder, name)) return;
	if (appendOwnedEntry(folder, { .kind = kind, .name = std::string(name) }))
		listed_names.insert(std::string(name));
}

////////////////////////// NO LOCK INTERNALS //////////////////////////

fs::FilePath artifacts::ArtifactCollection::entryPath(std::string_view name) const {
	// An output can be asked for anywhere, e.g. `--output-file-name /abs/path/app` from `duck`:
	// such a name is its own path. It is never owned (see `staysInsideCollection`), so the cache
	// writes it but does not remove it later.
	fs::FilePath path{ std::string(name) };
	if (path.hasRoot() || !path.isRelative()) return path;
	return folder / path;
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionNewNoLock(
	base::StrID collection_name
) {
	CORE_ASSERT(
		!sub_collections.contains(collection_name), "Sub-collection with this name already exists"
	);
	const auto new_path = entryPath(collection_name.strView());
	recordOwnedEntry(OWNED_COLLECTION, collection_name.strView());
	if (!new_path.exists()) {
		// A folder that cannot be made - no permission, a read-only mount - costs this build its
		// cache there, nothing more: whatever is written to the collection fails and is reported
		// by the writer.
		const auto created = fs::FileManager::createPhysicalFolder(new_path.absolute());
		if (!created.has_value())
			CORE_USER_LOG("Warning: cannot create the artifacts folder ", created.error(), "\n");
	}
	auto collection = Box<ArtifactCollection>::fromPointer(new ArtifactCollection(new_path, this));
	Ref<ArtifactCollection> result = collection.refMut();
	sub_collections.put(collection_name, std::move(collection));
	return result;
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactNewNoLock(base::StrID artifact_name
) {
	CORE_ASSERT(!file_artifacts.contains(artifact_name), "Duplicated file artifact");
	recordOwnedEntry(OWNED_FILE, artifact_name.strView());
	FileArtifact artifact{
		.parent = this,
		.name   = artifact_name,
		// Absolute: the path is handed to tools and recorded in debug info, which must not depend
		// on the working directory.
		.path = entryPath(artifact_name.strView()).absolute(),
	};
	file_artifacts.put(artifact_name, artifact);
	return artifact;
}

base::Optional<artifacts::FileArtifact> artifacts::ArtifactCollection::findAndMarkUsedNoLock(
	base::StrID artifact_name
) {
	auto maybe_artifact = file_artifacts.atMaybe(artifact_name);
	if (!maybe_artifact.has_value()) return {};
	FileArtifact artifact = *maybe_artifact.value();

	const auto touched = fs::FileManager::touch(artifact.path);
	if (!touched.has_value()) {
		// Only a file that is gone - e.g. removed by another build - is a miss. One that merely
		// cannot be marked as used (a read-only mount, no write permission) is still a valid
		// cache entry; at worst it is removed early as unused, and rebuilt.
		if (!artifact.path.exists()) {
			file_artifacts.erase(artifact_name);
			return {};
		}
		CORE_DEV_LOG(Artifacts, "Could not mark an artifact as used: ", touched.error(), "\n");
	}
	return artifact;
}

bool artifacts::ArtifactCollection::deleteFileArtifactNoLock(base::StrID artifact_name) {
	auto maybe_artifact = file_artifacts.atMaybe(artifact_name);
	if (!maybe_artifact.has_value()) return false;
	const auto removed = fs::FileManager::removeEntry(maybe_artifact.value()->path);
	if (!removed.has_value()) {
		// Still on disk, so still ours: keep it, and the list keeps naming it.
		CORE_USER_LOG("Warning: failed to remove artifact ", removed.error(), "\n");
		return false;
	}
	file_artifacts.erase(artifact_name);
	return removed.value();
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactNewNoLock(base::StrID artifact_name
) {
	CORE_ASSERT(!blob_artifacts.contains(artifact_name), "Duplicated blob artifact");
	BlobArtifact blob{ .parent = this, .name = artifact_name };
	blob_artifacts.put(artifact_name, blob);
	blob_data.put(artifact_name, makeBox<Bytes>());
	return blob;
}
