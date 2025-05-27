#include <artifacts/artifacts.hpp>
#include <filesystem/file.hpp>

#include <cassert>
#include <fstream>

constexpr int VALUE = 42;

int main() {
	fs::FilePath root = fs::FilePath::createTempDirectory();

	const auto b0 = base::StrID("b0");
	const auto f0 = base::StrID("f0");
	const auto s0 = base::StrID("s0");

	{
		// Create a collection
		artifacts::ArtifactCollection collection(root);

		// Create a simple blob artifact
		auto blob0 = collection.blobArtifactNew(b0);
		blob0.setData<decltype(VALUE)>(VALUE);
		assert(blob0.getData<decltype(VALUE)>() == VALUE);

		// Create a file artifact
		auto          file0 = collection.fileArtifactNew(f0);
		std::ofstream file(file0.FILE.absolutePath());
		file << "Hello!\n";
		file.close();

		auto sub_collection = collection.subCollectionNew(s0);
		auto sub_blob0      = sub_collection->blobArtifactNew(b0);
		sub_blob0.setData(VALUE + 1);

		// Flush the collection before removing the object to save the blobs.
		collection.flush();
	}
	{
		// Restore a collection
		artifacts::ArtifactCollection collection(root);

		assert(collection.blobArtifactAt(b0).getData<decltype(VALUE)>() == VALUE);

		auto          file0 = collection.fileArtifactAt(f0);
		std::ifstream file(file0.FILE.absolutePath());
		std::string   data;
		file >> data;
		file.close();
		assert(data == "Hello!");

		auto sub = collection.subCollectionAt(s0);
		assert(sub->blobArtifactAt(b0).getData<decltype(VALUE)>() == VALUE + 1);
	}

	return 0;
}
