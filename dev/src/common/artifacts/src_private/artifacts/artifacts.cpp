// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>

#include <artifacts/artifacts.hpp>
#include <artifacts/build_id.hpp>
#include <artifacts/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
#include <logger/logger.hpp>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

constexpr char ARTC_DELIM = ';';

namespace {
	/**
	 * @brief Reads one `ARTC_DELIM`-terminated decimal field out of a `.artc` body.
	 *
	 * Fails on a missing, empty, non-numeric or too-large field - which is exactly what the
	 * tail of a torn `.artc` looks like. `std::stoull` used to be called here instead, and it
	 * answers such input by throwing, which took the whole compiler down.
	 *
	 * @return False if no well-formed field could be read; `out` is then untouched.
	 */
	bool readDelimitedU32(std::stringstream& content, u32& out) {
		std::string buffer;
		if (!std::getline(content, buffer, ARTC_DELIM)) return false;

		const char* begin = buffer.data();
		const char* end   = begin + buffer.size();

		u64 value             = 0;
		const auto [stop, ec] = std::from_chars(begin, end, value);
		// The whole field has to be the number: a trailing character means the file is not
		// what we think it is.
		if (ec != std::errc{} || stop != end) return false;
		if (value > std::numeric_limits<u32>::max()) return false;

		out = static_cast<u32>(value);
		return true;
	}

	/**
	 * @brief Whether `n_bytes` more bytes can still be read from `content`.
	 *
	 * Size fields come straight from the file, so a corrupted one can claim up to 4 GiB. Checking
	 * it against what is actually left keeps such a field from becoming an allocation that size.
	 */
	bool fitsInRemaining(std::stringstream& content, u32 n_bytes) {
		const auto pos = content.tellg();
		if (pos < 0) return false;
		return n_bytes <= content.view().size() - static_cast<usize>(pos);
	}

	/// The first line of every `OWNED_LIST_FILE`: says what the file is and its format version.
	constexpr std::string_view OWNED_LIST_HEADER = "duckling-artifacts 1";

	constexpr char OWNED_FILE       = 'f';
	constexpr char OWNED_COLLECTION = 'c';

	struct OwnedEntry final {
		char        kind;
		std::string name;
	};

	std::string ownedListLine(char kind, std::string_view name) {
		return std::string{ kind, ' ' } + std::string(name) + '\n';
	}

	/**
	 * @brief Whether `name`, joined to a collection's directory, stays inside that directory.
	 *
	 * The list is read back from disk, so it is not trusted to keep a cleanup inside the cache:
	 * an absolute name or a `..` would point it at files that are not ours.
	 */
	bool staysInsideCollection(std::string_view name) {
		if (name.empty() || name.find('\n') != std::string_view::npos) return false;
		const std::filesystem::path path(name);
		if (path.has_root_path()) return false;
		return std::none_of(path.begin(), path.end(), [](const auto& part) { return part == ".."; });
	}

	/**
	 * @brief Reads the entries of `dir`'s `OWNED_LIST_FILE`.
	 * @return Nothing if the file is missing or is not a list we know how to read - then `dir`
	 * is not a collection of ours.
	 */
	base::Optional<std::vector<OwnedEntry>> readOwnedList(const std::filesystem::path& dir) {
		std::ifstream file(dir / artifacts::ArtifactCollection::OWNED_LIST_FILE, std::ios::binary);
		std::string   line;
		if (!std::getline(file, line) || line != OWNED_LIST_HEADER) return {};

		std::vector<OwnedEntry>         entries;
		std::unordered_set<std::string> seen;
		while (std::getline(file, line)) {
			// A last line without its newline was cut short by a killed build, and a cut name
			// can be the name of some other file - so it is not trusted.
			if (file.eof()) break;
			const bool well_formed = line.size() > 2 && line[1] == ' '
			                      && (line[0] == OWNED_FILE || line[0] == OWNED_COLLECTION);
			if (!well_formed) continue;
			auto name = line.substr(2);
			if (!staysInsideCollection(name) || !seen.insert(name).second) continue;
			entries.push_back({ .kind = line[0], .name = std::move(name) });
		}
		return entries;
	}

	/// The `.artc` blob cache of the collection kept in `dir`.
	std::filesystem::path artcPathOf(const std::filesystem::path& dir) {
		return dir / (dir.filename().string() + ".artc");
	}

	/// Whether `file_name` is a temporary file `writeFileAtomically` makes when writing `target`.
	bool isTempFileOf(std::string_view file_name, std::string_view target) {
		return file_name.size() > target.size() + 1 && file_name.starts_with(target)
		    && file_name[target.size()] == '.' && file_name.ends_with(".tmp");
	}

	/**
	 * @brief Removes `path`, warning if that fails.
	 * @return False if `path` could not be removed; a missing `path` counts as removed.
	 */
	bool removeOrWarn(const std::filesystem::path& path) {
		std::error_code ec;
		std::filesystem::remove(path, ec);
		if (!ec) return true;
		CORE_USER_LOG(
			"Warning: failed to remove stale artifact entry '",
			path.string(),
			"': ",
			ec.message(),
			"\n"
		);
		return false;
	}

	/**
	 * @brief Removes what the cache owns in `dir`, and nothing else: the entries its
	 * `OWNED_LIST_FILE` names (descending into owned sub-collections), then the cache's own
	 * files - the `.artc`, the build id, leftover temporary files and the list itself.
	 *
	 * @return False if something owned could not be removed. The list is then kept, so the
	 * next cleanup still knows about it.
	 */
	bool removeOwnedContents(const std::filesystem::path& dir) {
		bool all_removed = true;
		for (const auto& entry: readOwnedList(dir).copyValueOrDefault()) {
			const auto      path = dir / entry.name;
			std::error_code ec;
			// `symlink_status`, so that a link is removed rather than followed out of the cache.
			const auto status = std::filesystem::symlink_status(path, ec);
			if (ec || !std::filesystem::exists(status)) continue;

			if (entry.kind == OWNED_COLLECTION) {
				if (!std::filesystem::is_directory(status)) continue;
				all_removed &= removeOwnedContents(path);
				// Fails if something is left inside - which is not ours, so the folder stays.
				std::filesystem::remove(path, ec);
			} else if (!std::filesystem::is_directory(status)) {
				all_removed &= removeOrWarn(path);
			}
		}

		const auto                         artc_name = artcPathOf(dir).filename().string();
		std::vector<std::filesystem::path> temp_files;
		std::error_code                    iter_ec;
		for (const auto& entry: std::filesystem::directory_iterator(dir, iter_ec)) {
			const auto name = entry.path().filename().string();
			if (isTempFileOf(name, artc_name)
			    || isTempFileOf(name, artifacts::ArtifactCollection::OWNED_LIST_FILE))
				temp_files.push_back(entry.path());
		}
		for (const auto& temp_file: temp_files) removeOrWarn(temp_file);

		all_removed &= removeOrWarn(dir / artc_name);
		all_removed &= removeOrWarn(dir / artifacts::ArtifactCollection::BUILD_ID_FILE);
		// The list goes last: while anything it names is still there, it has to stay.
		if (all_removed)
			all_removed = removeOrWarn(dir / artifacts::ArtifactCollection::OWNED_LIST_FILE);
		return all_removed;
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

	if (has_file && stored_id == BUILD_ID) {
		flush_build_id = false;
		return;
	}

	if (has_file) {
		CORE_USER_LOG(
			"The artifacts cache at '",
			PATH.string(),
			"' was produced by a different build of the compiler (stored build id: ",
			stored_id.empty() ? std::string("<empty>") : stored_id.substr(0, 12) + "...",
			", current build id: ",
			std::string(BUILD_ID).substr(0, 12),
			"...). Removing the cached artifacts and starting fresh.\n"
		);
	} else if (std::filesystem::exists(PATH / OWNED_LIST_FILE)) {
		CORE_USER_LOG(
			"The artifacts cache at '",
			PATH.string(),
			"' has no build id. Removing the cached artifacts and starting fresh.\n"
		);
	}

	discardOwnedContents();
}

void artifacts::ArtifactCollection::discardOwnedContents() {
	removeOwnedContents(PATH);
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

void artifacts::ArtifactCollection::writeBuildIdFile() {
	if (flush_build_id) {
		const auto name     = base::StrID(std::string(BUILD_ID_FILE));
		const auto artifact = fileArtifactAtOrNewNoLock(name);
		// Losing the marker is self-correcting rather than dangerous: the next build finds no
		// build id, says so and clears the folder, which costs one full rebuild. So warn and
		// let this build finish.
		const auto written = artifact.file.writeToFile(BUILD_ID);
		if (!written.has_value())
			reportCacheWriteFailure(artifact.file.getFilePath(), written.error());
	}
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

base::OkBad artifacts::ArtifactCollection::parseBlobsFromBytes(std::stringstream& content) {
	u32 blob_count = 0;
	if (!readDelimitedU32(content, blob_count)) return base::BAD;

	u32 read_blobs = 0;
	while (read_blobs < blob_count && content && !content.eof()) {
		// Get name size.
		u32 name_size = 0;
		if (!readDelimitedU32(content, name_size)) return base::BAD;

		// Get the name. A size larger than what is left means the file ends mid-name.
		if (!fitsInRemaining(content, name_size)) return base::BAD;
		std::string blob_name(name_size, 'a');
		content.read(blob_name.data(), name_size);
		if (content.gcount() != std::streamsize(name_size)) return base::BAD;

		// Get data size.
		u32 blob_data_size = 0;
		if (!readDelimitedU32(content, blob_data_size)) return base::BAD;

		// Get the data.
		if (!fitsInRemaining(content, blob_data_size)) return base::BAD;
		std::vector<byte> blob_data(blob_data_size);
		content.read(reinterpret_cast<char*>(blob_data.data()), blob_data_size);
		if (content.gcount() != std::streamsize(blob_data_size)) return base::BAD;

		// Save the blob.
		auto blob = blobArtifactNew(base::StrID(blob_name.data()));
		blob.setData(blob_data.data(), blob_data.size());

		read_blobs++;
	}
	// Fewer blobs than the header promised means the file was cut short.
	return read_blobs == blob_count ? base::OK : base::BAD;
}

void artifacts::ArtifactCollection::loadData() {
	// Without a list this folder is not a collection of ours, so whatever is in it - an `.artc`
	// included - is not ours to load. Such an `.artc` can be a leftover from a cache older than
	// the list, written by another build of the compiler; flushing overwrites it.
	const auto maybe_owned = readOwnedList(PATH);
	if (!maybe_owned.has_value()) return;
	const auto& owned = maybe_owned.value();
	for (const auto& entry: owned) listed_names.insert(entry.name);

	// Read blob data.
	auto artc_file_path = getArtcFile();
	if (artc_file_path.exists()) {
		std::ifstream artc_file(artc_file_path.getPath());
		if (!artc_file) {
			discardUnusableCache("cannot be read");
			return;
		}

		std::stringstream content;
		content << artc_file.rdbuf();
		const auto parsed = parseBlobsFromBytes(content);
		artc_file.close();

		if (parsed.isBad()) {
			discardUnusableCache("is corrupted");
			return;
		}
	}

	// Attach the file artifacts and sub-collections. Only what the list names is ours: anything
	// else in the folder belongs to the user or to another tool.
	for (const auto& entry: owned) {
		std::error_code ec;
		const auto      status = std::filesystem::symlink_status(PATH / entry.name, ec);
		if (ec) continue;
		const auto name = base::StrID(entry.name);
		if (entry.kind == OWNED_COLLECTION && std::filesystem::is_directory(status))
			subCollectionNew(name);
		else if (entry.kind == OWNED_FILE && std::filesystem::is_regular_file(status))
			fileArtifactNew(name);
	}
}

void artifacts::ArtifactCollection::discardUnusableCache(std::string_view what_is_wrong) {
	CORE_USER_LOG(
		"The artifacts cache at '",
		PATH.string(),
		"' ",
		what_is_wrong,
		". Removing the cached artifacts and starting fresh.\n"
	);
	discardOwnedContents();
}

void artifacts::ArtifactCollection::flushDown() {
	writeArtcFile();
	writeOwnedList();

	// Every sub-collection gets its turn whatever happened above: one directory we cannot write
	// to must not cost the whole tree its cache.
	for (auto&& [_, sub_collection]: sub_collections) sub_collection->flushDown();
}

void artifacts::ArtifactCollection::writeArtcFile() const {
	const auto artc_file_path = getArtcFile();
	CORE_DEV_LOG(
		Artifacts,
		"Flushing ArtifactCollection at: ",
		std::filesystem::absolute(artc_file_path),
		"\n"
	);

	writeFileAtomically(artc_file_path.getPath(), [this](std::ostream& file) {
		file << std::to_string(blob_artifacts.size()) << ARTC_DELIM;
		for (const auto& [blob_name, blob]: blob_artifacts) {
			const auto& data = blob_data[blob_name];
			// Name
			file << blob_name.strView().size() << ARTC_DELIM << blob_name.strView();
			// Content
			file << data->size() << ARTC_DELIM;
			file.write(
				reinterpret_cast<const char*>(data->data()), base::safeIntConv<u32>(data->size())
			);
		}
	});
}

void artifacts::ArtifactCollection::writeOwnedList() {
	std::vector<OwnedEntry> entries;
	for (const auto& [name, _]: file_artifacts)
		entries.push_back({ .kind = OWNED_FILE, .name = std::string(name.strView()) });
	for (const auto& [name, _]: sub_collections)
		entries.push_back({ .kind = OWNED_COLLECTION, .name = std::string(name.strView()) });
	// Another process sharing this folder may have created entries this one never loaded.
	for (auto& entry: readOwnedList(PATH).copyValueOrDefault()) entries.push_back(std::move(entry));

	std::unordered_set<std::string> kept;
	std::string                     content = std::string(OWNED_LIST_HEADER) + '\n';
	for (const auto& entry: entries) {
		if (!staysInsideCollection(entry.name) || kept.contains(entry.name)) continue;
		// An entry whose file is gone (e.g. through `deleteFileArtifact`) has nothing left to
		// own, so it is dropped here instead of piling up.
		std::error_code ec;
		if (!std::filesystem::exists(std::filesystem::symlink_status(PATH / entry.name, ec)))
			continue;
		kept.insert(entry.name);
		content += ownedListLine(entry.kind, entry.name);
	}

	writeFileAtomically(PATH / OWNED_LIST_FILE, [&content](std::ostream& file) { file << content; });
	listed_names = std::move(kept);
}

void artifacts::ArtifactCollection::recordOwnedEntry(char kind, std::string_view name) {
	if (!staysInsideCollection(name) || listed_names.contains(std::string(name))) return;

	const auto list_path = fs::FilePath(PATH / OWNED_LIST_FILE);
	// Appending, not rewriting: each entry is one short write, so entries that concurrent
	// processes append to the same list do not overwrite each other.
	std::ofstream list(list_path.getPath(), std::ios::binary | std::ios::app);
	if (!list) {
		reportCacheWriteFailure(list_path, "could not be opened for writing");
		return;
	}
	// A new list starts with its header. Two processes may both find the list empty and both
	// write one; a reader skips the second as a malformed entry.
	list.seekp(0, std::ios::end);
	if (list.tellp() == 0) list << OWNED_LIST_HEADER << '\n';
	list << ownedListLine(kind, name);
	list.close();
	if (!list) {
		reportCacheWriteFailure(list_path, "could not be written");
		return;
	}
	listed_names.insert(std::string(name));
}

void artifacts::ArtifactCollection::writeFileAtomically(
	const std::filesystem::path& path, const std::function<void(std::ostream&)>& write_content
) {
	// Every writer gets its own temporary file. With a shared name, two processes flushing the
	// same folder would write into the same file, and the first rename would publish their
	// bytes mixed together.
	const auto tmp_file_path = fs::FileManager::randomName(
		fs::FilePath(path.parent_path()), path.filename().string() + ".", ".tmp"
	);

	std::ofstream file(tmp_file_path.getPath(), std::ios::binary | std::ios::trunc);
	if (!file) {
		reportCacheWriteFailure(tmp_file_path, "could not be opened for writing");
		return;
	}
	write_content(file);
	file.close();

	// Only a fully written temporary file earns the rename; anything else is thrown away, which
	// is what keeps the previous file intact.
	if (!file) {
		reportCacheWriteFailure(tmp_file_path, "could not be written");
		std::error_code remove_ec;
		std::filesystem::remove(tmp_file_path.getPath(), remove_ec);
		return;
	}

	std::error_code rename_ec;
	std::filesystem::rename(tmp_file_path.getPath(), path, rename_ec);
	if (rename_ec) {
		reportCacheWriteFailure(fs::FilePath(path), rename_ec.message());
		std::error_code remove_ec;
		std::filesystem::remove(tmp_file_path.getPath(), remove_ec);
	}
}

void artifacts::ArtifactCollection::reportCacheWriteFailure(
	const fs::FilePath& path, std::string_view reason
) {
	// Losing the cache costs the next build its incremental speed-up, nothing more: the
	// artifacts this build produced are already on disk. So the user is told and the build is
	// allowed to finish - but told loudly, because silence here used to be how a full disk
	// turned into a mysteriously slow or broken project.
	CORE_USER_LOG(
		"Warning: could not save the artifacts cache '",
		path.string(),
		"': ",
		reason,
		". The next build will not be able to reuse it.\n"
	);
}

fs::FilePath artifacts::ArtifactCollection::getArtcFile() const { return artcPathOf(PATH); }

void artifacts::ArtifactCollection::flushNoLock() {
	if (PARENT) {
		PARENT.value()->flush();
	} else {
		flushDown();
		if constexpr (CHECK_BUILD_ID) writeBuildIdFile();
	}
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionNewNoLock(
	base::StrID collection_name
) {
	CORE_ASSERT(
		!sub_collections.contains(collection_name), "Sub-collection with this name already exists"
	);
	auto new_path = PATH / collection_name.strView();
	recordOwnedEntry(OWNED_COLLECTION, collection_name.strView());
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

	recordOwnedEntry(OWNED_FILE, artifact_name.strView());
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
}

base::RawView artifacts::ArtifactCollection::getBlobDataViewNoLock(const BlobArtifact& blob) const {
	CORE_ASSERT(blob.parent.get() == this, "Blob does not belong to this collection");
	const auto& data = blob_data[blob.name];
	return { data->data(), data->size() };
}
