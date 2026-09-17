#include <lsp_interface/files_cache.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <string>

class LspFilesCacheTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspFilesCacheTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(incrementalSyncTest);
		TESTER_ADD_TEST(multiByteSyncTest);
		TESTER_ADD_TEST(wholeDocumentSyncTest);
		TESTER_ADD_TEST(changeOutOfRangeTest);
		TESTER_ADD_TEST(outOfOrderVersionTest);
	}

private:
	/**
	 * @brief Applies `changes` to the buffer of a document opened with `initial`, and returns
	 * what the buffer holds afterwards.
	 */
	std::string updateDocumentWithChanges(
		std::string_view initial, const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
	) {
		duck_ls::FilesCache files;

		auto uri        = lsp::Uri::parse("file:///ws/splice/a.dk");
		auto cache_path = files.openDocument(uri, "duckling", 1, initial);
		assertTrue(!cache_path.empty(), "The document must open");

		assertTrue(
			files.updateDocument(uri, 2, changes).isOk(), "The changes must apply to the buffer"
		);

		return std::string{ fs::File(cache_path.value()).getContent().view().stringView() };
	}

	/**
	 * @brief A partial change replacing `[start, end)` on the given lines with `text`.
	 */
	lsp::TextDocumentContentChangeEvent partialChange(
		u32 start_line, u32 start_character, u32 end_line, u32 end_character, std::string text
	) {
		return lsp::TextDocumentContentChangePartial{
			.range = { .start = { .line = start_line, .character = start_character },
			           .end   = { .line = end_line, .character = end_character } },
			.text  = std::move(text),
		};
	}

	/**
	 * @brief A change replacing the whole buffer with `text`.
	 */
	lsp::TextDocumentContentChangeEvent wholeDocument(std::string text) {
		return lsp::TextDocumentContentChangeWholeDocument{ .text = std::move(text) };
	}

	void incrementalSyncTest() {
		// Changes apply in order, so the second one sees the result of the first.
		auto updated = updateDocumentWithChanges(
			"line one\nline two\n",
			{ partialChange(0, 5, 0, 8, "ONE"),
		      partialChange(1, 0, 1, 4, "LINE"),
		      partialChange(2, 0, 2, 0, "tail") }
		);

		assertTrue(updated == "line ONE\nLINE two\ntail", "Wrong buffer after the edits");
	}

	void multiByteSyncTest() {
		// "źó" is two code points of two bytes each, so a UTF-16 column is not a byte offset.
		auto result = updateDocumentWithChanges("źód\n", { partialChange(0, 2, 0, 3, "D") });

		assertTrue(result == "źóD\n", "Wrong buffer after a multi byte edit");
	}

	void wholeDocumentSyncTest() {
		auto result = updateDocumentWithChanges("old\n", { wholeDocument("new\n") });

		assertTrue(result == "new\n", "A whole document change must replace the buffer");
	}

	void changeOutOfRangeTest() {
		duck_ls::FilesCache files;

		auto uri        = lsp::Uri::parse("file:///ws/splice/b.dk");
		auto cache_path = files.openDocument(uri, "duckling", 1, "one line\n");
		assertTrue(!cache_path.empty(), "The document must open");

		assertTrue(
			files.updateDocument(uri, 2, { partialChange(9, 0, 9, 1, "x") }).isBad(),
			"A change outside the buffer must be rejected"
		);
		assertTrue(
			fs::File(cache_path.value()).getContent().view().stringView() == "one line\n",
			"A rejected change must leave the buffer untouched"
		);
	}

	void outOfOrderVersionTest() {
		duck_ls::FilesCache files;

		auto uri        = lsp::Uri::parse("file:///ws/splice/c.dk");
		auto cache_path = files.openDocument(uri, "duckling", 2, "abc\n");
		assertTrue(!cache_path.empty(), "The document must open");

		assertTrue(
			files.updateDocument(uri, 2, { partialChange(0, 0, 0, 1, "A") }).isBad(),
			"A repeated version must be rejected"
		);
		assertTrue(
			fs::File(cache_path.value()).getContent().view().stringView() == "abc\n",
			"A rejected change must leave the buffer untouched"
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
