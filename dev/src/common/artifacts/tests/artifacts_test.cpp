// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <artifacts/artifacts.hpp>
#include <artifacts/build_id.hpp>
#include <tester/tester.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

constexpr int VALUE = 42;

class ArtifactsTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ArtifactsTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(buildIdMismatchRemovesOnlyOwnedArtifacts);
		TESTER_ADD_TEST(missingBuildIdRemovesOwnedArtifacts);
		TESTER_ADD_TEST(foreignFolderIsLeftAlone);
		TESTER_ADD_TEST(corruptedArtcIsDiscarded);
		TESTER_ADD_TEST(oversizedSizeFieldIsRejected);
		TESTER_ADD_TEST(leftoverTempFilesAreNotAdopted);
		TESTER_ADD_TEST(unflushedArtifactsAreRemoved);
		TESTER_ADD_TEST(truncatedListLineIsIgnored);
		TESTER_ADD_TEST(artcWithoutOwnedListIsNotLoaded);
		TESTER_ADD_TEST(dotEntryIsIgnored);
		TESTER_ADD_TEST(normalizedNamesAreOwned);
		TESTER_ADD_TEST(artifactOutsideTheCacheIsNotOwned);
		TESTER_ADD_TEST(entriesThroughSymlinksAreSkipped);
		TESTER_ADD_TEST(relativeRootWorks);
		TESTER_ADD_TEST(unusedFileArtifactsAreRemoved);
		TESTER_ADD_TEST(reusedArtifactsAreKept);
		TESTER_ADD_TEST(reuseOfRemovedFileIsAMiss);
		TESTER_ADD_TEST(removeCacheRemovesOnlyOwned);
		TESTER_ADD_TEST(removeCacheRemovesLeftoverTempFiles);
		TESTER_ADD_TEST(staleTempFilesAreRemoved);
		TESTER_ADD_TEST(listEntriesOutsideTheCacheAreIgnored);
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

	static void writeFile(const std::filesystem::path& path, std::string_view content) {
		std::ofstream out(path, std::ios::binary);
		out << content;
	}

	static std::string readFile(const std::filesystem::path& path) {
		std::ifstream     in(path, std::ios::binary);
		std::stringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}

	static std::filesystem::path artcOf(const std::filesystem::path& dir) {
		return dir / (dir.filename().string() + ".artc");
	}

	static bool hasCurrentBuildId(const std::filesystem::path& root) {
		return readFile(root / artifacts::ArtifactCollection::BUILD_ID_FILE)
		    == std::string(artifacts::BUILD_ID);
	}

	/**
	 * Builds and flushes a cache with one of each kind of artifact: blob `b0`, file `f0`, and
	 * sub-collection `s0` holding blob `b0` and file `f1`.
	 */
	static void buildCache(const std::filesystem::path& root) {
		artifacts::ArtifactCollection collection(root);
		collection.blobArtifactNew(base::StrID("b0")).setData(VALUE);
		writeFile(collection.fileArtifactNew(base::StrID("f0")).file.getFilePath().getPath(), "f0");
		auto sub = collection.subCollectionNew(base::StrID("s0"));
		sub->blobArtifactNew(base::StrID("b0")).setData(VALUE + 1);
		writeFile(sub->fileArtifactNew(base::StrID("f1")).file.getFilePath().getPath(), "f1");
		collection.flush();
	}

	/// Puts files the cache does not own next to the ones it does, at both levels.
	static void addUserFiles(const std::filesystem::path& root) {
		writeFile(root / "user_file", "keep me");
		std::filesystem::create_directory(root / "user_dir");
		writeFile(root / "user_dir" / "inside", "keep me");
		writeFile(root / "s0" / "user_file", "keep me");
	}

	void assertUserFilesKept(const std::filesystem::path& root) {
		ASSERT_TRUE(readFile(root / "user_file") == "keep me");
		ASSERT_TRUE(readFile(root / "user_dir" / "inside") == "keep me");
		ASSERT_TRUE(readFile(root / "s0" / "user_file") == "keep me");
	}

	void assertOwnedArtifactsRemoved(
		const std::filesystem::path& root, artifacts::ArtifactCollection& collection
	) {
		ASSERT_TRUE(!std::filesystem::exists(root / "f0"));
		ASSERT_TRUE(!std::filesystem::exists(root / "s0" / "f1"));
		ASSERT_TRUE(!std::filesystem::exists(artcOf(root / "s0")));
		ASSERT_TRUE(collection.blobArtifactAtMaybe(base::StrID("b0")).empty());
		ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("f0")).empty());
		ASSERT_TRUE(collection.subCollectionAtMaybe(base::StrID("s0")).empty());
	}

	void buildIdMismatchRemovesOnlyOwnedArtifacts() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);
		writeFile(
			root / artifacts::ArtifactCollection::BUILD_ID_FILE,
			"stale-build-id-that-cannot-match-a-real-sha256"
		);

		{
			artifacts::ArtifactCollection collection(root);
			assertOwnedArtifactsRemoved(root, collection);
			assertUserFilesKept(root);
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("user_file")).empty());
			collection.flush();
		}
		ASSERT_TRUE(hasCurrentBuildId(root));
		// The build id is the cache's own bookkeeping, not an artifact.
		ASSERT_TRUE(
			readFile(root / artifacts::ArtifactCollection::OWNED_LIST_FILE).find(".build_id")
			== std::string::npos
		);
	}

	void missingBuildIdRemovesOwnedArtifacts() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);
		std::filesystem::remove(root / artifacts::ArtifactCollection::BUILD_ID_FILE);

		artifacts::ArtifactCollection collection(root);
		assertOwnedArtifactsRemoved(root, collection);
		assertUserFilesKept(root);
	}

	// The `-a .` case: the artifacts folder is a folder of the user's own files.
	void foreignFolderIsLeftAlone() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		writeFile(root / "main.dk", "source");
		std::filesystem::create_directory(root / "src");
		writeFile(root / "src" / "lib.dk", "source");

		for (int build = 0; build < 3; build++) {
			artifacts::ArtifactCollection collection(root);
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("main.dk")).empty());
			ASSERT_TRUE(collection.subCollectionAtMaybe(base::StrID("src")).empty());
			collection.flush();
		}
		ASSERT_TRUE(readFile(root / "main.dk") == "source");
		ASSERT_TRUE(readFile(root / "src" / "lib.dk") == "source");
		// The user's folders are never taken for sub-collections, so nothing is written there.
		ASSERT_TRUE(!std::filesystem::exists(artcOf(root / "src")));
	}

	void corruptedArtcIsDiscarded() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);
		writeFile(artcOf(root), "5;");

		{
			artifacts::ArtifactCollection collection(root);
			assertOwnedArtifactsRemoved(root, collection);
			assertUserFilesKept(root);
			collection.blobArtifactNew(base::StrID("b0")).setData(VALUE);
			collection.flush();
		}
		// The build id went with the corrupted cache and has to be written back; otherwise the
		// next build would discard the fresh cache again.
		ASSERT_TRUE(hasCurrentBuildId(root));
		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(
			collection.blobArtifactAt(base::StrID("b0")).getData<decltype(VALUE)>() == VALUE
		);
	}

	// A size field that claims more bytes than the file has must be rejected before anything
	// that size is allocated.
	void oversizedSizeFieldIsRejected() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		writeFile(artcOf(root), "1;4000000000;");

		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(collection.blobArtifactAtMaybe(base::StrID("b0")).empty());
	}

	void leftoverTempFilesAreNotAdopted() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		const auto temp_name = artcOf(root).filename().string() + ".Leftover12345678.tmp";
		writeFile(root / temp_name, "half a cache");
		const auto build_id_temp_name
			= std::string(artifacts::ArtifactCollection::BUILD_ID_FILE) + ".Leftover12345678.tmp";
		writeFile(root / build_id_temp_name, "half a build id");

		{
			artifacts::ArtifactCollection collection(root);
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID(temp_name)).empty());
		}
		// Clearing the cache also takes the leftover with it.
		std::filesystem::remove(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(!std::filesystem::exists(root / temp_name));
		ASSERT_TRUE(!std::filesystem::exists(root / build_id_temp_name));
	}

	// Entries are recorded when they are created, so a build that dies before flushing still
	// leaves nothing the next cleanup does not know about.
	void unflushedArtifactsAreRemoved() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		{
			artifacts::ArtifactCollection collection(root);
			collection.fileArtifactNew(base::StrID("f0"));
			collection.subCollectionNew(base::StrID("s0"))->fileArtifactNew(base::StrID("f1"));
		}
		ASSERT_TRUE(std::filesystem::exists(root / "f0"));

		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(!std::filesystem::exists(root / "f0"));
		ASSERT_TRUE(!std::filesystem::exists(root / "s0"));
	}

	// A list cut short mid-line by a killed build must not have its last, cut name trusted:
	// `main.o` cut to `main` would point at a file that is not ours.
	void truncatedListLineIsIgnored() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		writeFile(
			root / artifacts::ArtifactCollection::OWNED_LIST_FILE, "duckling-artifacts 1\nf main"
		);
		writeFile(root / "main", "user binary");

		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(readFile(root / "main") == "user binary");
	}

	// A cache older than the owned list has no list anywhere. When it is cleared, its
	// sub-collection folders are left alone - but the `.artc` in them was written by another
	// build of the compiler and must not be loaded into a collection of that name.
	void artcWithoutOwnedListIsNotLoaded() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		std::filesystem::remove(root / artifacts::ArtifactCollection::OWNED_LIST_FILE);
		std::filesystem::remove(root / "s0" / artifacts::ArtifactCollection::OWNED_LIST_FILE);
		writeFile(
			root / artifacts::ArtifactCollection::BUILD_ID_FILE,
			"stale-build-id-that-cannot-match-a-real-sha256"
		);

		artifacts::ArtifactCollection collection(root);
		auto                          sub = collection.subCollectionNew(base::StrID("s0"));
		ASSERT_TRUE(sub->blobArtifactAtMaybe(base::StrID("b0")).empty());
	}

	// `.` names the collection's own folder: taken for a sub-collection, it would recurse into
	// itself until the path got too long, and the build would crash on every run.
	void dotEntryIsIgnored() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		for (const auto* name: { ".", "./", "s0/..", "./." }) {
			writeFile(
				root / artifacts::ArtifactCollection::OWNED_LIST_FILE,
				std::string("duckling-artifacts 1\nc s0\nc ") + name + "\n"
			);
			{
				artifacts::ArtifactCollection collection(root);
				collection.flush();
			}
			// The discard path walks the list too.
			std::filesystem::remove(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
			artifacts::ArtifactCollection collection(root);
			collection.flush();
		}
	}

	// A name that only looks different (`./f0`) is still ours.
	void normalizedNamesAreOwned() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		{
			artifacts::ArtifactCollection collection(root);
			collection.fileArtifactNew(base::StrID("./f0"));
			collection.flush();
		}
		std::filesystem::remove(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(!std::filesystem::exists(root / "f0"));
	}

	// An output asked for outside the artifacts folder (`--output-file-name ../app`) is written
	// there, but clearing the cache must not remove it.
	void artifactOutsideTheCacheIsNotOwned() {
		std::filesystem::path outer
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		const auto root = outer / "cache";
		std::filesystem::create_directory(root);
		{
			artifacts::ArtifactCollection collection(root);
			writeFile(
				collection.fileArtifactNew(base::StrID("../app")).file.getFilePath().getPath(),
				"user binary"
			);
			collection.flush();
		}
		ASSERT_TRUE(
			readFile(root / artifacts::ArtifactCollection::OWNED_LIST_FILE).find("app")
			== std::string::npos
		);
		std::filesystem::remove(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(readFile(outer / "app") == "user binary");
	}

	// The system follows a symlink in the middle of a path silently, so `link/victim` with `link`
	// pointing out of the cache names a file outside it. Entries reached through a symlink are
	// not ours, wherever the symlink sits in the path.
	void entriesThroughSymlinksAreSkipped() {
		std::filesystem::path outer
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		const auto root = outer / "cache";
		std::filesystem::create_directory(root);
		std::filesystem::create_directory(outer / "elsewhere");
		writeFile(outer / "elsewhere" / "victim", "keep me");
		writeFile(outer / "target", "keep me");
		std::filesystem::create_directory_symlink(outer / "elsewhere", root / "link");
		std::filesystem::create_symlink(outer / "target", root / "file_link");
		std::filesystem::create_directory_symlink(outer / "elsewhere", root / "dir_link");
		// A matching build id, so that the first block really goes through `loadData`; without
		// one, the cleanup would remove the list before `loadData` ever saw it.
		buildCache(root);
		writeFile(
			root / artifacts::ArtifactCollection::OWNED_LIST_FILE,
			"duckling-artifacts 1\nf link/victim\nf file_link\nc dir_link\n"
		);

		{
			artifacts::ArtifactCollection collection(root);
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("link/victim")).empty());
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("file_link")).empty());
			ASSERT_TRUE(collection.subCollectionAtMaybe(base::StrID("dir_link")).empty());
		}
		// No build id: the cleanup runs over the list, and must leave all of it alone.
		std::filesystem::remove(root / artifacts::ArtifactCollection::BUILD_ID_FILE);
		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(readFile(outer / "elsewhere" / "victim") == "keep me");
		ASSERT_TRUE(readFile(outer / "target") == "keep me");
		ASSERT_TRUE(std::filesystem::is_symlink(root / "file_link"));
		ASSERT_TRUE(std::filesystem::is_symlink(root / "dir_link"));
	}

	// The default artifacts folder is relative (`./duck_build/`), so every path the cache builds
	// is relative too.
	void relativeRootWorks() {
		std::filesystem::path outer
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		const auto old_cwd = std::filesystem::current_path();
		std::filesystem::current_path(outer);
		std::filesystem::create_directory("cache");
		const std::filesystem::path root("./cache/");
		buildCache(root);
		writeFile(artcOf(root), "5;");
		{
			artifacts::ArtifactCollection collection(root);
			collection.flush();
		}
		const bool has_build_id = hasCurrentBuildId(root);
		std::filesystem::current_path(old_cwd);
		ASSERT_TRUE(has_build_id);
		ASSERT_TRUE(!std::filesystem::exists(outer / "cache" / "f0"));
	}

	static void makeUnusedFor(const std::filesystem::path& path, std::chrono::days days) {
		std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() - days);
	}

	// Artifacts nobody has written or reused for the given time are removed, in sub-collections
	// too; recently used ones, and files that are not ours, stay.
	void unusedFileArtifactsAreRemoved() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);
		makeUnusedFor(root / "f0", std::chrono::days(8));
		makeUnusedFor(root / "s0" / "f1", std::chrono::days(8));
		makeUnusedFor(root / "user_file", std::chrono::days(8));
		{
			artifacts::ArtifactCollection collection(root);
			writeFile(
				collection.fileArtifactNew(base::StrID("fresh")).file.getFilePath().getPath(), "x"
			);
			ASSERT_TRUE(collection.removeFileArtifactsUnusedFor(std::chrono::days(7)) == 2);
			ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("f0")).empty());
			collection.flush();
		}
		ASSERT_TRUE(!std::filesystem::exists(root / "f0"));
		ASSERT_TRUE(!std::filesystem::exists(root / "s0" / "f1"));
		ASSERT_TRUE(std::filesystem::exists(root / "fresh"));
		assertUserFilesKept(root);
		// The removed artifacts are dropped from the list on flush.
		ASSERT_TRUE(
			readFile(root / artifacts::ArtifactCollection::OWNED_LIST_FILE).find("f0")
			== std::string::npos
		);
	}

	// Reusing an artifact marks it as used, which keeps it from the cleanup.
	void reusedArtifactsAreKept() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		makeUnusedFor(root / "f0", std::chrono::days(8));

		artifacts::ArtifactCollection collection(root);
		auto                          reused = collection.fileArtifactForReuse(base::StrID("f0"));
		ASSERT_HAS_VALUE(reused);
		ASSERT_TRUE(collection.removeFileArtifactsUnusedFor(std::chrono::days(7)) == 0);
		ASSERT_TRUE(readFile(root / "f0") == "f0");
		ASSERT_TRUE(collection.fileArtifactForReuse(base::StrID("missing")).empty());
	}

	// A file another build removed after this one loaded the collection is a cache miss, not a
	// file artifact pointing at nothing.
	void reuseOfRemovedFileIsAMiss() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);

		artifacts::ArtifactCollection collection(root);
		std::filesystem::remove(root / "f0");
		ASSERT_TRUE(collection.fileArtifactForReuse(base::StrID("f0")).empty());
		ASSERT_TRUE(!std::filesystem::exists(root / "f0"));
		ASSERT_TRUE(collection.fileArtifactAtMaybe(base::StrID("f0")).empty());
	}

	// `duckc clean`: everything the cache owns goes, nothing else does.
	void removeCacheRemovesOnlyOwned() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);

		ASSERT_TRUE(artifacts::ArtifactCollection::removeCache(root).isOk());
		ASSERT_TRUE(!std::filesystem::exists(root / "f0"));
		ASSERT_TRUE(!std::filesystem::exists(root / "s0" / "f1"));
		ASSERT_TRUE(!std::filesystem::exists(artcOf(root)));
		ASSERT_TRUE(!std::filesystem::exists(root / artifacts::ArtifactCollection::BUILD_ID_FILE));
		ASSERT_TRUE(!std::filesystem::exists(root / artifacts::ArtifactCollection::OWNED_LIST_FILE));
		assertUserFilesKept(root);
	}

	// A killed write leaves `<name>.<random>.tmp` next to an artifact; it belongs to the artifact.
	// Files that only look similar, or sit next to the user's files, are not ours.
	void removeCacheRemovesLeftoverTempFiles() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);
		writeFile(root / "f0.Killed1234567890.tmp", "half");
		writeFile(root / "s0" / "f1.Killed4567890123.tmp", "half");
		writeFile(root / "user_file.Killed7890123456.tmp", "keep me");
		writeFile(root / "f0.backup.copy.tmp", "keep me");
		// The user's backup of our artifact: the part between the dots is not our random part.
		writeFile(root / "f0.backup.tmp", "keep me");

		ASSERT_TRUE(artifacts::ArtifactCollection::removeCache(root).isOk());
		ASSERT_TRUE(!std::filesystem::exists(root / "f0.Killed1234567890.tmp"));
		ASSERT_TRUE(!std::filesystem::exists(root / "s0" / "f1.Killed4567890123.tmp"));
		ASSERT_TRUE(readFile(root / "user_file.Killed7890123456.tmp") == "keep me");
		ASSERT_TRUE(readFile(root / "f0.backup.copy.tmp") == "keep me");
		ASSERT_TRUE(readFile(root / "f0.backup.tmp") == "keep me");
	}

	// In a normal build, only leftovers too old to belong to a write still in progress go.
	void staleTempFilesAreRemoved() {
		std::filesystem::path root
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		buildCache(root);
		addUserFiles(root);
		writeFile(root / "f0.OldWrite00000001.tmp", "half");
		writeFile(root / "s0" / "f1.OldWrite00000002.tmp", "half");
		writeFile(root / "f0.FreshWrite000001.tmp", "in progress");
		writeFile(root / "user_file.OldWrite00000003.tmp", "keep me");
		writeFile(root / "f0.backup.tmp", "keep me");
		makeUnusedFor(root / "f0.backup.tmp", std::chrono::days(2));
		for (const auto& old: { root / "f0.OldWrite00000001.tmp",
		                        root / "s0" / "f1.OldWrite00000002.tmp",
		                        root / "user_file.OldWrite00000003.tmp" })
			makeUnusedFor(old, std::chrono::days(2));

		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(collection.removeTemporaryFilesOlderThan(std::chrono::days(1)) == 2);
		ASSERT_TRUE(!std::filesystem::exists(root / "f0.OldWrite00000001.tmp"));
		ASSERT_TRUE(!std::filesystem::exists(root / "s0" / "f1.OldWrite00000002.tmp"));
		ASSERT_TRUE(readFile(root / "f0.FreshWrite000001.tmp") == "in progress");
		ASSERT_TRUE(readFile(root / "user_file.OldWrite00000003.tmp") == "keep me");
		ASSERT_TRUE(readFile(root / "f0.backup.tmp") == "keep me");
	}

	// Names are read back from disk, so the cleanup must not follow one out of the cache.
	void listEntriesOutsideTheCacheAreIgnored() {
		std::filesystem::path outer
			= fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		const auto root = outer / "cache";
		std::filesystem::create_directory(root);
		writeFile(outer / "outside", "keep me");
		writeFile(
			root / artifacts::ArtifactCollection::OWNED_LIST_FILE,
			"duckling-artifacts 1\nf ../outside\nf " + (outer / "outside").string() + "\n"
		);

		artifacts::ArtifactCollection collection(root);
		ASSERT_TRUE(readFile(outer / "outside") == "keep me");
	}
};

TESTER_COMMON_MAIN("/src/common/artifacts/tests/");
