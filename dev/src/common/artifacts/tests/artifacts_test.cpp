// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <artifacts/artifacts.hpp>
#include <artifacts/build_id.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

constexpr int VALUE = 42;

class ArtifactsTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ArtifactsTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(buildIdMismatchWipesArtifacts);
	}

private:
	struct SimpleStruct {
		float     x;
		long long z;
		bool      operator==(const SimpleStruct& other) const noexcept = default;
	};

	void simpleTest() {
		fs::File              fs_root_path = fs::FileManager::createRandomTempDirectory();
		std::filesystem::path root         = fs_root_path.getFilePath().getPath();

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
			ASSERT_TRUE(blob0.getData<decltype(VALUE)>() == VALUE);

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

			ASSERT_TRUE(collection.blobArtifactAtOrNew(b0).getData<decltype(VALUE)>() == VALUE);

			ASSERT_TRUE(collection.blobArtifactAtOrNew(b1).getData<SimpleStruct>() == simple_struct);

			auto          file0 = collection.fileArtifactAt(f0);
			std::ifstream file(file0.file.getFilePath().getPath());
			std::string   data;
			file >> data;
			file.close();
			ASSERT_TRUE(data == "Hello!");

			auto sub = collection.subCollectionAt(s0);
			ASSERT_TRUE(sub->blobArtifactAt(b0).getData<decltype(VALUE)>() == VALUE + 1);
		}
	}

	void buildIdMismatchWipesArtifacts() {
		fs::File              fs_root_path = fs::FileManager::createRandomTempDirectory();
		std::filesystem::path root         = fs_root_path.getFilePath().getPath();

		// Write a stale build id file.
		{
			std::ofstream out(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
			out << "stale-build-id-that-cannot-match-a-real-sha256";
		}

		// Drop a stray file and a stray sub-directory that should be wiped.
		{
			std::ofstream out(root / "stray_file");
			out << "should be deleted";
		}
		const auto stray_dir = root / "stray_dir";
		std::filesystem::create_directory(stray_dir);
		{
			std::ofstream out(stray_dir / "inside");
			out << "should be deleted";
		}

		// Construct root - should wipe stale contents.
		{
			artifacts::ArtifactCollection collection(root);
			ASSERT_TRUE(!std::filesystem::exists(root / "stray_file"));
			ASSERT_TRUE(!std::filesystem::exists(stray_dir));
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("stray_file")).empty());

			// Flush should (re)write the .build_id with the current BUILD_ID.
			collection.flush();
		}

		std::ifstream in(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
		ASSERT_TRUE(in.is_open());
		std::stringstream ss;
		ss << in.rdbuf();
		ASSERT_TRUE(ss.str() == std::string(artifacts::BUILD_ID));
	}
};

TESTER_COMMON_MAIN("/src/common/artifacts/tests/");
