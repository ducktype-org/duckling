// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <artifacts/artifacts.hpp>
#include <filesystem/file.hpp>

#include <cassert>
#include <fstream>

constexpr int VALUE = 42;

struct SimpleStruct {
	float     x;
	long long z;
	bool      operator==(const SimpleStruct& other) const noexcept = default;
};

int main() {
	fs::File              fs_root_file = fs::FileManager::createRandomTempDirectory();
	std::filesystem::path root         = fs_root_file.getFilePath().getPath();

	const auto b0 = base::StrID("b0");
	const auto b1 = base::StrID("b1");
	const auto f0 = base::StrID("f0");
	const auto s0 = base::StrID("s0");

	SimpleStruct simple_struct{ .x = 10.5, .z = 50 };
	{
		// Create a collection
		artifacts::ArtifactCollection collection(root);

		// Create a simple blob artifact
		auto blob0 = collection.blobArtifactNew(b0);
		blob0.setData<decltype(VALUE)>(VALUE);
		assert(blob0.getData<decltype(VALUE)>().value() == VALUE);

		// Save struct
		auto blob1 = collection.blobArtifactNew(b1);
		blob1.setData(simple_struct);

		// Create a file artifact
		auto          file0 = collection.fileArtifactNew(f0);
		std::ofstream file(file0.file.getFilePath().getPath());
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

		assert(collection.blobArtifactAt(b0).getData<decltype(VALUE)>().value() == VALUE);

		assert(collection.blobArtifactAt(b1).getData<SimpleStruct>().value() == simple_struct);

		auto          file0 = collection.fileArtifactAt(f0);
		std::ifstream file(file0.file.getFilePath().getPath());
		std::string   data;
		file >> data;
		file.close();
		assert(data == "Hello!");

		auto sub = collection.subCollectionAt(s0);
		assert(sub->blobArtifactAt(b0).getData<decltype(VALUE)>().value() == VALUE + 1);
	}

	return 0;
}
