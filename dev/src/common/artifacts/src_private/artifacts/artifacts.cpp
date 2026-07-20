#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/misc/int_conv.hpp>

#include <artifacts/artifacts.hpp>
#include <artifacts/build_id.hpp>
#include <artifacts/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
#include <logger/logger.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <utility>

constexpr char ARTC_DELIM = ';';

namespace {
	/**
	 * @brief Writes to `path` atomically: `write_content` is handed an output stream over a
	 * uniquely-named temp file in the same directory, which is then renamed over `path`. `rename`
	 * within one directory/filesystem is atomic, so a concurrent reader of `path` always sees
	 * either the old or the new content in full, never a truncated/partial write. `write_content`
	 * writes straight to the temp file stream, so callers with large payloads (e.g. blob data)
	 * don't need to buffer the whole content in a separate in-memory copy first.
	 */
	template<typename WriteContentFn>
	void writeFileAtomically(const std::filesystem::path& path, WriteContentFn&& write_content) {
		static thread_local std::mt19937_64 rng{ std::random_device{}() };
		const auto tmp_path = path.string() + ".tmp-" + std::to_string(rng());

		{
			std::ofstream tmp_file(tmp_path, std::ios::out | std::ios::trunc | std::ios::binary);
			if (!tmp_file) CORE_PANIC("Failed to open temp file for atomic write: " + tmp_path);
			std::forward<WriteContentFn>(write_content)(tmp_file);
			if (!tmp_file) CORE_PANIC("Failed to write temp file for atomic write: " + tmp_path);
		}

		std::error_code ec;
		std::filesystem::rename(tmp_path, path, ec);
		if (ec) {
			std::error_code remove_ec;
			std::filesystem::remove(tmp_path, remove_ec);
			CORE_PANIC(
				"Failed to atomically move '" + tmp_path + "' to '" + path.string()
				+ "': " + ec.message()
			);
		}
	}

	/** @brief writeFileAtomically overload for callers that already have the content as a string. */
	void writeFileAtomically(const std::filesystem::path& path, std::string_view content) {
		writeFileAtomically(path, [&](std::ostream& out) {
			out.write(content.data(), base::safeIntConv<std::streamsize>(content.size()));
		});
	}
}

void artifacts::BlobArtifact::setData(const byte* ptr, usize n_bytes) {
	parent->setBlobData(*this, ptr, n_bytes);
}

base::RawView artifacts::BlobArtifact::getDataView() const {
	return parent->getBlobDataView(*this);
}

//////////// COLLECTION EXTERNALLY VISIBLE METHODS ////////////


artifacts::ArtifactCollection::ArtifactCollection(std::filesystem::path root):
	  PATH(std::move(root)) {
	CORE_ASSERT(std::filesystem::exists(PATH), "ArtifactCollection path does not exist");
	CORE_ASSERT(std::filesystem::is_directory(PATH), "ArtifactCollection path is not a directory");
	if constexpr (CHECK_BUILD_ID) validateOrWipeBuildId();
	loadData();
}

void artifacts::ArtifactCollection::validateOrWipeBuildId() {
	const fs::FilePath build_id_path = PATH / BUILD_ID_FILE;
	std::string        stored_id;
	const bool         has_file = build_id_path.exists();
	if (has_file) {
		const auto content = fs::File(build_id_path).getContent();
		stored_id          = content.view().stdString();
	}

	if (has_file && stored_id == BUILD_ID) return;

	// Contents are stale, foreign, or missing a marker entirely: this run must (re)write a
	// fresh `.build_id` once it flushes, even if it ends up compiling nothing itself.
	dirty = true;

	if (has_file) {
		CORE_USER_LOG(
			"Artifacts at '",
			PATH.string(),
			"' were produced by a different build of the compiler (stored build id: ",
			stored_id.empty() ? std::string("<empty>") : stored_id.substr(0, 12) + "...",
			", current build id: ",
			std::string(BUILD_ID).substr(0, 12),
			"...). Clearing whole cache folder and starting fresh.\n"
		);
	} else if (fs::File(PATH).listFilePaths().size() > 0) {
		CORE_USER_LOG(
			"Artifacts at '",
			PATH.string(),
			"' do not contain a build-id marker. Clearing whole cache folder and starting fresh.\n"
		);
	}

	std::error_code iter_ec;
	for (const auto& entry: std::filesystem::directory_iterator(PATH, iter_ec)) {
		std::error_code remove_ec;
		std::filesystem::remove_all(entry.path(), remove_ec);
		if (remove_ec) {
			CORE_USER_LOG(
				"Warning: failed to remove stale artifact entry '",
				entry.path().string(),
				"': ",
				remove_ec.message(),
				"\n"
			);
		}
	}
	if (iter_ec) {
		CORE_USER_LOG(
			"Warning: failed to iterate artifacts directory '",
			PATH.string(),
			"' while clearing cache: ",
			iter_ec.message(),
			"\n"
		);
	}
}

void artifacts::ArtifactCollection::writeBuildIdFile() {
	// Write atomically first, then register the FileArtifact: fileArtifactAtOrNewNoLock only
	// creates an (empty) placeholder file when none exists yet, so writing first means it
	// always finds the real content already in place and never exposes a transient empty file.
	writeFileAtomically(PATH / BUILD_ID_FILE, BUILD_ID);
	fileArtifactAtOrNewNoLock(base::StrID(std::string(BUILD_ID_FILE)));
}

void artifacts::ArtifactCollection::flush() {
	WithLock lock(*this);
	this->flushNoLock();
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionNew(
	base::StrID collection_name
) {
	WithLock lock(*this);
	return subCollectionNewNoLock(collection_name);
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAtOrNew(
	base::StrID collection_name
) {
	WithLock lock(*this);
	return subCollectionAtOrNewNoLock(collection_name);
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAt(
	base::StrID collection_name
) {
	WithLock lock(*this);
	return subCollectionAtNoLock(collection_name);
}

base::Optional<Ref<artifacts::ArtifactCollection>> artifacts::ArtifactCollection::subCollectionAtMaybe(
	base::StrID collection_name
) {
	WithLock lock(*this);
	return subCollectionAtMaybeNoLock(collection_name);
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactNew(base::StrID artifact_name) {
	WithLock lock(*this);
	return fileArtifactNewNoLock(artifact_name);
}

bool artifacts::ArtifactCollection::deleteFileArtifact(base::StrID artifact_name) {
	WithLock lock(*this);
	return deleteFileArtifactNoLock(artifact_name);
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactAtOrNew(base::StrID artifact_name
) {
	WithLock lock(*this);
	return fileArtifactAtOrNewNoLock(artifact_name);
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactAt(base::StrID artifact_name
) const {
	WithLock lock(*this);
	return fileArtifactAtNoLock(artifact_name);
}

base::Optional<base::CRef<artifacts::FileArtifact>> artifacts::ArtifactCollection::fileArtifactAtMaybe(
	base::StrID artifact_name
) const {
	WithLock lock(*this);
	return fileArtifactAtMaybeNoLock(artifact_name);
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactNew(base::StrID artifact_name) {
	WithLock lock(*this);
	return blobArtifactNewNoLock(artifact_name);
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactAtOrNew(base::StrID artifact_name
) {
	WithLock lock(*this);
	return blobArtifactAtOrNewNoLock(artifact_name);
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactAt(base::StrID artifact_name
) const {
	WithLock lock(*this);
	return blobArtifactAtNoLock(artifact_name);
}

base::Optional<base::CRef<artifacts::BlobArtifact>> artifacts::ArtifactCollection::blobArtifactAtMaybe(
	base::StrID artifact_name
) const {
	WithLock lock(*this);
	return blobArtifactAtMaybeNoLock(artifact_name);
}

void artifacts::ArtifactCollection::setBlobData(
	const BlobArtifact& blob, const byte* ptr, usize n_bytes
) {
	WithLock lock(*this);
	setBlobDataNoLock(blob, ptr, n_bytes);
}

base::RawView artifacts::ArtifactCollection::getBlobDataView(const BlobArtifact& blob) const {
	WithLock lock(*this);
	return getBlobDataViewNoLock(blob);
}

/////////////////////// INTERNAL METHODS //////////////////////


artifacts::ArtifactCollection::ArtifactCollection(
	std::filesystem::path root, Ref<ArtifactCollection> parent
):
	  PATH(std::move(root)),
	  PARENT(parent) {
	loadData();
}

void artifacts::ArtifactCollection::parseBlobsFromBytes(std::stringstream& content) {
	std::string buffer;

	u32 blob_count = 0;
	std::getline(content, buffer, ARTC_DELIM);
	blob_count = base::safeIntConv<u32>(std::stoull(buffer));

	u32 read_blobs = 0;
	while (read_blobs < blob_count && content && !content.eof()) {
		// Get name size.
		u32 name_size = 0;
		std::getline(content, buffer, ARTC_DELIM);
		name_size = base::safeIntConv<u32>(std::stoull(buffer));

		// Get the name.
		std::string blob_name(name_size, 'a');
		content.read(blob_name.data(), name_size);

		// Get data size.
		u32 blob_data_size = 0;
		std::getline(content, buffer, ARTC_DELIM);
		blob_data_size = base::safeIntConv<u32>(std::stoull(buffer));

		// Get the data.
		std::vector<byte> blob_data(blob_data_size);
		content.read(reinterpret_cast<char*>(blob_data.data()), blob_data_size);

		// Save the blob.
		auto blob = blobArtifactNew(base::StrID(blob_name.data()));
		blob.setData(blob_data.data(), blob_data.size());

		read_blobs++;
	}
	CORE_ASSERT(blob_count == read_blobs, "Read invalid blob count");
}

void artifacts::ArtifactCollection::loadData() {
	// Reconstructing existing on-disk state below (via blobArtifactNew/setData/fileArtifactNew)
	// must not be mistaken for new writes that need flushing.
	is_loading = true;
	defer(is_loading = false);

	// Read blob data.
	auto artc_file_path = getArtcFile();
	if (artc_file_path.exists()) {
		std::ifstream     artc_file(artc_file_path.getPath());
		std::stringstream content;
		content << artc_file.rdbuf();
		parseBlobsFromBytes(content);
		artc_file.close();

		// Read file artifacts and other sub-collections.
		for (const auto& inner_path: std::filesystem::directory_iterator(PATH)) {
			const auto filename = inner_path.path().filename().string();
			const auto name     = base::StrID(filename);
			if (inner_path.is_directory())
				subCollectionNew(name);
			else if (inner_path.is_regular_file())
				fileArtifactNew(name);
		}
	}
}

void artifacts::ArtifactCollection::flushDown() {
	auto artc_file_path = getArtcFile();
	CORE_DEV_LOG(
		Artifacts,
		"Flushing ArtifactCollection at: ",
		std::filesystem::absolute(artc_file_path),
		"\n"
	);

	// Written to a temp file and renamed into place so a concurrent reader constructing an
	// ArtifactCollection at this path never observes a half-written .artc blob. Streamed directly
	// to the temp file rather than staged in an in-memory buffer first, since blob data (e.g.
	// cached LLVM IR/object bytes) can be large enough that a full extra copy matters.
	writeFileAtomically(artc_file_path.getPath(), [&](std::ostream& content) {
		content << std::to_string(blob_artifacts.size()) << ARTC_DELIM;
		for (const auto& [blob_name, blob]: blob_artifacts) {
			const auto& data = blob_data[blob_name];
			// Name
			content << blob_name.strView().size() << ARTC_DELIM << blob_name.strView();
			// Content
			content << data->size() << ARTC_DELIM;
			content.write(
				reinterpret_cast<const char*>(data->data()), base::safeIntConv<u32>(data->size())
			);
		}
	});

	for (auto&& [_, sub_collection]: sub_collections) sub_collection->flushDown();
}

fs::FilePath artifacts::ArtifactCollection::getArtcFile() const {
	return PATH / (PATH.filename().string() + ".artc");
}

void artifacts::ArtifactCollection::flushNoLock() {
	if (PARENT) {
		PARENT.value()->flush();
	} else {
		if (!dirty) return;
		flushDown();
		if constexpr (CHECK_BUILD_ID) writeBuildIdFile();
		dirty = false;
	}
}

void artifacts::ArtifactCollection::markDirty() {
	WithLock lock(*this);
	markDirtyNoLock();
}

void artifacts::ArtifactCollection::markDirtyNoLock() {
	// Non-root collections don't track their own dirty state (only the root's is consulted by
	// flushNoLock(), which always delegates flushing up to the root); just propagate up. Uses
	// the locking markDirty(), since the ancestor's lock is not held here.
	if (PARENT)
		PARENT.value()->markDirty();
	else
		dirty = true;
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionNewNoLock(
	base::StrID collection_name
) {
	CORE_ASSERT(
		!sub_collections.contains(collection_name), "Sub-collection with this name already exists"
	);
	auto new_path = PATH / collection_name.strView();
	if (!std::filesystem::exists(new_path)) std::filesystem::create_directory(new_path);
	sub_collections.put(
		collection_name, Box<ArtifactCollection>::fromPointer(new ArtifactCollection(new_path, this))
	);
	return subCollectionAtNoLock(collection_name);
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAtOrNewNoLock(
	base::StrID collection_name
) {
	match_optional(subCollectionAtMaybeNoLock(collection_name)) {
		opt_some(collection) return collection;
		opt_none return subCollectionNewNoLock(collection_name);
	}
	CORE_UNREACHABLE();
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAtNoLock(
	base::StrID collection_name
) {
	return sub_collections.at(collection_name).refMut();
}

base::Optional<Ref<artifacts::ArtifactCollection>> artifacts::ArtifactCollection::subCollectionAtMaybeNoLock(
	base::StrID collection_name
) {
	return sub_collections.atMaybe(collection_name).map([](const auto& ref) {
		return ref->refMut();
	});
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactNewNoLock(base::StrID artifact_name
) {
	CORE_ASSERT(!file_artifacts.contains(artifact_name), "Duplicated blob artifact");
	auto file_path = PATH / artifact_name.strView();

	if (!std::filesystem::exists(file_path)) std::ofstream(file_path).close();  // create the file

	file_artifacts.put(
		artifact_name,
		FileArtifact{
			.parent = this,
			.name   = artifact_name,
			.file   = fs::File(file_path),
		}
	);
	return fileArtifactAtNoLock(artifact_name);
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactAtOrNewNoLock(
	base::StrID artifact_name
) {
	match_optional(fileArtifactAtMaybeNoLock(artifact_name)) {
		opt_some(artifact) return *artifact;
		opt_none return fileArtifactNewNoLock(artifact_name);
	}
	CORE_UNREACHABLE();
}

artifacts::FileArtifact artifacts::ArtifactCollection::fileArtifactAtNoLock(base::StrID artifact_name
) const {
	return file_artifacts.at(artifact_name);
}

base::Optional<base::CRef<artifacts::FileArtifact>> artifacts::ArtifactCollection::fileArtifactAtMaybeNoLock(
	base::StrID artifact_name
) const {
	return file_artifacts.atMaybe(artifact_name);
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactNewNoLock(base::StrID artifact_name
) {
	CORE_ASSERT(!blob_artifacts.contains(artifact_name), "Duplicated blob artifact");
	blob_artifacts.put(artifact_name, BlobArtifact{ .parent = this, .name = artifact_name });
	blob_data.put(artifact_name, makeBox<Bytes>());
	if (!is_loading) markDirtyNoLock();
	return blobArtifactAtNoLock(artifact_name);
}

bool artifacts::ArtifactCollection::deleteFileArtifactNoLock(base::StrID artifact_name) {
	auto maybe_file_artifact = fileArtifactAtMaybeNoLock(artifact_name);
	if (!maybe_file_artifact.has_value()) return false;
	auto file_artifact = *maybe_file_artifact.value();
	bool result        = fs::FileManager::deleteFile(file_artifact.file);
	file_artifacts.erase(artifact_name);
	return result;
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactAtOrNewNoLock(
	base::StrID artifact_name
) {
	match_optional(blobArtifactAtMaybeNoLock(artifact_name)) {
		opt_some(artifact) return *artifact;
		opt_none return blobArtifactNewNoLock(artifact_name);
	}
	CORE_UNREACHABLE();
}

artifacts::BlobArtifact artifacts::ArtifactCollection::blobArtifactAtNoLock(base::StrID artifact_name
) const {
	return blob_artifacts.at(artifact_name);
}

base::Optional<base::CRef<artifacts::BlobArtifact>> artifacts::ArtifactCollection::blobArtifactAtMaybeNoLock(
	base::StrID artifact_name
) const {
	return blob_artifacts.atMaybe(artifact_name);
}

void artifacts::ArtifactCollection::setBlobDataNoLock(
	const BlobArtifact& blob, const byte* ptr, usize n_bytes
) {
	CORE_ASSERT(blob.parent.get() == this, "Blob does not belong to this collection");
	blob_data[blob.name] = makeBox<Bytes>(ptr, ptr + n_bytes);
	if (!is_loading) markDirtyNoLock();
}

base::RawView artifacts::ArtifactCollection::getBlobDataViewNoLock(const BlobArtifact& blob) const {
	CORE_ASSERT(blob.parent.get() == this, "Blob does not belong to this collection");
	const auto& data = blob_data[blob.name];
	return { data->data(), data->size() };
}
