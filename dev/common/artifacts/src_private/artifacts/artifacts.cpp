#include <artifacts/artifacts.hpp>

#include <utility>

void artifacts::BlobArtifact::setData(const byte* ptr, usize n_bytes) {
	PARENT->setBlobData(*this, ptr, n_bytes);
}

base::RawView artifacts::BlobArtifact::getDataView() const {
	return PARENT->getBlobDataView(*this);
}

artifacts::ArtifactCollection::ArtifactCollection(fs::FilePath root): PATH(std::move(root)) {
	throw base::NotYetImplemented("loading files from disk");
}

artifacts::ArtifactCollection::ArtifactCollection(fs::FilePath root, Ref<ArtifactCollection> parent):
	  PARENT(parent),
	  PATH(std::move(root)) {
	throw base::NotYetImplemented("loading files from disk");
}

void artifacts::ArtifactCollection::flushDown() {
	auto file_name = PATH.name();
	throw base::NotYetImplemented("saving files to disk");

	for (auto&& [_, sub_collection]: sub_collections) sub_collection->flushDown();
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
	// @note For the reviewer: this is not enforced as I'm convinced it's needed.
	// Also, currently there is no `FilePath::getType()` method, but the field exists.
	// CORE_ASSERT(PATH.getType() == fs::FileType::Physical, "Not a physical path");

	CORE_ASSERT(
		!sub_collections.contains(collection_name), "Sub-collection with this name already exists"
	);
	auto new_path = PATH.createDirectoryIn(collection_name.strView());
	sub_collections.put(
		collection_name, Box<ArtifactCollection>::fromPointer(new ArtifactCollection(new_path, this))
	);
	return subCollectionAt(collection_name);
}

Ref<artifacts::ArtifactCollection> artifacts::ArtifactCollection::subCollectionAtOrNew(
	base::StrID collection_name
) {
	return subCollectionAtMaybe(collection_name).valueOr(subCollectionNew(collection_name));
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
	auto file_path = PATH.createFileIn("");
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
	return fileArtifactAtMaybe(artifact_name).valueOr(fileArtifactNew(artifact_name));
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
	return blobArtifactAtMaybe(artifact_name).valueOr(blobArtifactNew(artifact_name));
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
