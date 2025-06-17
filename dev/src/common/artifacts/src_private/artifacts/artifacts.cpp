#include <artifacts/artifacts.hpp>

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>
#include <base/optional.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

constexpr char ARTC_DELIM = ';';

void artifacts::BlobArtifact::setData(const byte* ptr, usize n_bytes) {
	PARENT->setBlobData(*this, ptr, n_bytes);
}

base::RawView artifacts::BlobArtifact::getDataView() const {
	return PARENT->getBlobDataView(*this);
}

artifacts::ArtifactCollection::ArtifactCollection(std::filesystem::path root):
	  PATH(std::move(root)) {
	CORE_ASSERT(std::filesystem::exists(PATH), "ArtifactCollection path does not exist");
	CORE_ASSERT(
		std::filesystem::is_directory(PATH), "ArtifactCollection path is not a directory"
	);
	loadData();
}

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
	// Read blob data.
	auto artc_file_path = getArtcFile();
	if (exists(artc_file_path)) {
		std::ifstream     artc_file(artc_file_path);
		std::stringstream content;
		content << artc_file.rdbuf();
		parseBlobsFromBytes(content);
		artc_file.close();

		// Read file artifacts and other sub-collections.
		for (const auto& inner_path: std::filesystem::directory_iterator(PATH)) {
			auto name = base::StrID(inner_path.path().filename().c_str());
			if (inner_path.is_directory())
				subCollectionNew(name);
			else if (inner_path.is_regular_file())
				fileArtifactNew(name);
		}
	}
}

void artifacts::ArtifactCollection::flushDown() {
	auto artc_file_path = getArtcFile();
	std::cerr << "Flushing ArtifactCollection at: " << absolute(artc_file_path) << '\n';

	std::ofstream file(artc_file_path);
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
	file.close();

	for (auto&& [_, sub_collection]: sub_collections) sub_collection->flushDown();
}

std::filesystem::path artifacts::ArtifactCollection::getArtcFile() const {
	return PATH / (PATH.filename().string() + ".artc");
}

void artifacts::ArtifactCollection::flush() {
	if (PARENT)
		PARENT.value()->flush();
	else
		flushDown();
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionNew(
	base::StrID collection_name
) {
	CORE_ASSERT(
		!sub_collections.contains(collection_name), "Sub-collection with this name already exists"
	);
	auto new_path = PATH / collection_name.strView();
	if (!exists(new_path)) create_directory(new_path);
	sub_collections.put(
		collection_name, Box<ArtifactCollection>::fromPointer(new ArtifactCollection(new_path, this))
	);
	return subCollectionAt(collection_name);
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAtOrNew(
	base::StrID collection_name
) {
	match_optional(subCollectionAtMaybe(collection_name)) {
		opt_some(collection) return collection;
		opt_none return subCollectionNew(collection_name);
	}
	CORE_UNREACHABLE();
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAt(
	base::StrID collection_name
) {
	return sub_collections.at(collection_name).refMut();
}

base::Optional<Ref<artifacts::ArtifactCollection>> artifacts::ArtifactCollection::subCollectionAtMaybe(
	base::StrID collection_name
) {
	return sub_collections.atMaybe(collection_name).map([](const auto& ref) {
		return ref.refMut();
	});
}

const artifacts::FileArtifact& artifacts::ArtifactCollection::fileArtifactNew(
	base::StrID artifact_name
) {
	CORE_ASSERT(!file_artifacts.contains(artifact_name), "Duplicated blob artifact");
	auto file_path = PATH / artifact_name.strView();

	if (!exists(file_path)) std::ofstream(file_path).close();  // create the file

	file_artifacts.put(
		artifact_name,
		FileArtifact{
			.PARENT = this,
			.NAME   = artifact_name,
			.FILE   = file_path,
		}
	);
	return fileArtifactAt(artifact_name);
}

const artifacts::FileArtifact& artifacts::ArtifactCollection::fileArtifactAtOrNew(
	base::StrID artifact_name
) {
	match_optional(fileArtifactAtMaybe(artifact_name)) {
		opt_some(artifact) return artifact;
		opt_none return fileArtifactNew(artifact_name);
	}
	CORE_UNREACHABLE();
}

const artifacts::FileArtifact& artifacts::ArtifactCollection::fileArtifactAt(base::StrID artifact_name
) const {
	return file_artifacts.at(artifact_name);
}

base::Optional<const artifacts::FileArtifact&> artifacts::ArtifactCollection::fileArtifactAtMaybe(
	base::StrID artifact_name
) const {
	return file_artifacts.atMaybe(artifact_name);
}

const artifacts::BlobArtifact& artifacts::ArtifactCollection::blobArtifactNew(
	base::StrID artifact_name
) {
	CORE_ASSERT(!blob_artifacts.contains(artifact_name), "Duplicated blob artifact");
	blob_artifacts.put(artifact_name, BlobArtifact{ .PARENT = this, .NAME = artifact_name });
	blob_data.put(artifact_name, makeBox<Bytes>());
	return blobArtifactAt(artifact_name);
}

const artifacts::BlobArtifact& artifacts::ArtifactCollection::blobArtifactAtOrNew(
	base::StrID artifact_name
) {
	match_optional(blobArtifactAtMaybe(artifact_name)) {
		opt_some(artifact) return artifact;
		opt_none return blobArtifactNew(artifact_name);
	}
	CORE_UNREACHABLE();
}

const artifacts::BlobArtifact& artifacts::ArtifactCollection::blobArtifactAt(base::StrID artifact_name
) const {
	return blob_artifacts.at(artifact_name);
}

base::Optional<const artifacts::BlobArtifact&> artifacts::ArtifactCollection::blobArtifactAtMaybe(
	base::StrID artifact_name
) const {
	return blob_artifacts.atMaybe(artifact_name);
}

void artifacts::ArtifactCollection::setBlobData(
	const BlobArtifact& blob, const byte* ptr, usize n_bytes
) {
	CORE_ASSERT(blob.PARENT.get() == this, "Blob does not belong to this collection");
	blob_data[blob.NAME] = makeBox<Bytes>(ptr, ptr + n_bytes);
}

base::RawView artifacts::ArtifactCollection::getBlobDataView(const BlobArtifact& blob) const {
	CORE_ASSERT(blob.PARENT.get() == this, "Blob does not belong to this collection");
	const auto& data = blob_data[blob.NAME];
	return { data->data(), data->size() };
}
