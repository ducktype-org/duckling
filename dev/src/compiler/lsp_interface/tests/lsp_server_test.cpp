#include "string_stream.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/global_logger.hpp>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>

#include <diagnostic/logger.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

#include <string>
#include <vector>

namespace {
	/**
	 * @brief Records every call the handlers make instead of touching the real compiler.
	 */
	class RecordingCompiler final: public duck_ls::Compiler {
	public:
		using Compiler::Compiler;

		struct Call final {
			std::string name;
			std::string uri;
			/// For a change notification, how many changes it carried.
			usize change_count = 0;
		};

		std::vector<Call> calls;

		void addWorkspace(const lsp::Uri& root) override {
			calls.push_back({ "addWorkspace", root.toString(), 0 });
		}

		void openDocument(const lsp::Uri& uri, std::string_view, i32, std::string_view) override {
			calls.push_back({ "openDocument", uri.toString(), 0 });
		}

		base::OkBad updateDocument(
			const lsp::Uri&                                        uri,
			i32,
			const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
		) override {
			calls.push_back({ "updateDocument", uri.toString(), changes.size() });
			return base::OK;
		}

		base::OkBad closeDocument(const lsp::Uri& uri) override {
			calls.push_back({ "closeDocument", uri.toString(), 0 });
			return base::OK;
		}

		void fileCreatedOrDeletedOnDisk(const lsp::Uri& uri) override {
			calls.push_back({ "fileCreatedOrDeletedOnDisk", uri.toString(), 0 });
		}

		void publishDiagnostics(const lsp::Uri& uri) override {
			calls.push_back({ "publishDiagnostics", uri.toString(), 0 });
		}
	};

	std::string framed(const std::vector<std::string>& messages) {
		std::string input;
		for (const auto& message: messages) input += duck_ls_test::frame(message);
		return input;
	}

	/**
	 * @brief Feeds a scripted session to a session wired to a recording compiler.
	 */
	std::string runSession(
		const std::vector<std::string>& messages, std::vector<RecordingCompiler::Call>* out_calls
	) {
		duck_ls_test::StringStream stream(framed(messages));

		lsp::ServerEndpoint    endpoint(stream);
		duck_ls::ServerSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache    files;
		RecordingCompiler      compiler{ base::Ref<duck_ls::ServerSession>(&session),
		                                 base::Ref<duck_ls::FilesCache>(&files) };

		session.registerHandlers(compiler);
		endpoint.runMessageLoop();

		if (out_calls != nullptr) *out_calls = compiler.calls;

		return stream.written();
	}

	/**
	 * @brief Drops every space and newline, so assertions do not depend on json formatting.
	 */
	std::string compact(std::string_view text) {
		std::string result;
		for (char c: text)
			if (c != ' ' && c != '\t' && c != '\n' && c != '\r') result += c;
		return result;
	}
}

class LspServerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspServerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(lifecycleTest);
		TESTER_ADD_TEST(requestBeforeInitializeTest);
		TESTER_ADD_TEST(handlerDispatchTest);
		TESTER_ADD_TEST(incrementalSyncTest);
		TESTER_ADD_TEST(multiByteSyncTest);
		TESTER_ADD_TEST(wholeDocumentSyncTest);
		TESTER_ADD_TEST(changeOutOfRangeTest);
		TESTER_ADD_TEST(outOfOrderVersionTest);
		TESTER_ADD_TEST(swapCycleTest);
		TESTER_ADD_TEST(diagnosticsTest);
		TESTER_ADD_TEST(virtualWorkspaceTest);
	}

protected:
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
		query::setTrackReverseGraph(true);
		compiler::frontend::use_module_modifier_remove = true;
	}

private:
	void lifecycleTest() {
		auto written = compact(runSession(
			{
				R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})",
				R"({"jsonrpc":"2.0","method":"initialized","params":{}})",
				R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})",
				R"({"jsonrpc":"2.0","method":"exit"})",
			},
			nullptr
		));

		assertTrue(
			written.find(R"("openClose":true)") != std::string::npos,
			"initialize must advertise openClose: " + written
		);
		assertTrue(
			written.find(R"("change":2)") != std::string::npos,
			"initialize must advertise incremental sync: " + written
		);
		assertTrue(
			written.find(R"("name":"duck_ls")") != std::string::npos,
			"initialize must report the server name: " + written
		);
	}

	void requestBeforeInitializeTest() {
		auto written = compact(runSession(
			{
				R"({"jsonrpc":"2.0","id":1,"method":"shutdown"})",
				R"({"jsonrpc":"2.0","method":"exit"})",
			},
			nullptr
		));

		assertTrue(
			written.find("-32002") != std::string::npos,
			"A request before initialize must be rejected with ServerNotInitialized: " + written
		);
	}

	void handlerDispatchTest() {
		std::vector<RecordingCompiler::Call> calls;
		runSession(
			{
				R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":"file:///ws","capabilities":{}}})",
				R"({"jsonrpc":"2.0","method":"initialized","params":{}})",
				R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///ws/a.dk","languageId":"duckling","version":1,"text":"abc"}}})",
				R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///ws/a.dk","version":2},"contentChanges":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"text":"A"}]}})",
				R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":"file:///ws/b.dk","type":1}]}})",
				R"({"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"file:///ws/a.dk"}}})",
				R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})",
				R"({"jsonrpc":"2.0","method":"exit"})",
			},
			&calls
		);

		const std::vector<std::string> expected{
			"addWorkspace",   "openDocument",
			"publishDiagnostics", "updateDocument",
			"publishDiagnostics", "fileCreatedOrDeletedOnDisk",
			"publishDiagnostics", "closeDocument",
			"publishDiagnostics",
		};

		assertTrue(
			calls.size() == expected.size(),
			"Wrong number of compiler calls: " + std::to_string(calls.size())
		);
		for (usize i = 0; i < expected.size(); ++i)
			assertTrue(
				calls[i].name == expected[i],
				"Call " + std::to_string(i) + " must be " + expected[i] + ", was " + calls[i].name
			);

		assertTrue(calls[0].uri == "file:///ws", "The workspace root must be forwarded as sent");
		assertTrue(calls[1].uri == "file:///ws/a.dk", "The open call must name the document");
		assertTrue(calls[3].change_count == 1, "The change must carry its one edit");
	}

	/**
	 * @brief Splices `changes` into the buffer of a document opened with `initial`.
	 */
	std::string spliceIntoBuffer(
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

	void incrementalSyncTest() {
		// Changes apply in order, so the second one sees the result of the first.
		auto spliced = spliceIntoBuffer(
			"line one\nline two\n",
			{ partialChange(0, 5, 0, 8, "ONE"),
		      partialChange(1, 0, 1, 4, "LINE"),
		      partialChange(2, 0, 2, 0, "tail") }
		);

		assertTrue(spliced == "line ONE\nLINE two\ntail", "Wrong buffer after the edits");
	}

	void multiByteSyncTest() {
		// "źó" is two code points of two bytes each, so a UTF-16 column is not a byte offset.
		auto spliced = spliceIntoBuffer("źód\n", { partialChange(0, 2, 0, 3, "D") });

		assertTrue(spliced == "źóD\n", "Wrong buffer after a multi byte edit");
	}

	void wholeDocumentSyncTest() {
		auto spliced = spliceIntoBuffer(
			"old\n", { lsp::TextDocumentContentChangeWholeDocument{ .text = "new\n" } }
		);

		assertTrue(spliced == "new\n", "A whole document change must replace the buffer");
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

	/**
	 * @brief The file currently backing the module whose main source file is `path`.
	 */
	fs::File backingFileOf(const fs::FilePath& path) {
		using namespace compiler::frontend;

		auto source_files = SourceFile::getSourceFilesFromFile(fs::File(path));
		assertTrue(!source_files.empty(), "No source file is registered for " + path.string());

		auto module_id  = source_files.back()->getModule().illegalAccess().getID();
		auto module_ref = getModuleRef(module_id);
		return getFileRef(module_ref->getMainSourceFile().illegalAccess().getID())
		    ->getFileIllegalAccess();
	}

	/**
	 * @brief A compiler wired to a session that only collects what it was asked to push.
	 */
	struct CollectingSession final: public duck_ls::ServerSession {
		using ServerSession::ServerSession;

		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> pushed;

		void pushDiagnostics(
			const lsp::Uri& uri, const std::vector<lsp::Diagnostic>& diagnostics
		) override {
			pushed[uri] = diagnostics;
		}
	};

	void swapCycleTest() {
		// The tree is built next to the test binary rather than in the temp directory, because a
		// temp path is its own PathType and cannot be converted to a cache path.
		auto workspace_path
			= fs::FilePath(std::filesystem::current_path()).join("lsp_swap_physical_ws");
		std::filesystem::remove_all(workspace_path.getPath());

		auto workspace = fs::FileManager::createPhysicalFolder(workspace_path, true);
		auto package   = workspace.createSubDirectory("pkg");
		auto main      = package.createSubFile("fun main() -> i64 = { return 0; }\n", "pkg.dk");
		auto helper = package.createSubFile("fun helper() -> i64 = { return 1; }\n", "helper.dk");
		(void) helper;

		const auto  main_file = main.getFilePath();
		const std::string disk_text{ fs::File(main_file).getContent().view().stringView() };

		duck_ls_test::StringStream stream("");
		lsp::ServerEndpoint        endpoint(stream);
		CollectingSession          session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache        files;
		duck_ls::Compiler   compiler{ base::Ref<duck_ls::ServerSession>(&session),
		                              base::Ref<duck_ls::FilesCache>(&files) };

		auto root_uri = lsp::Uri::fileUriFromPath(workspace.getFilePath().genericString());
		auto file_uri = lsp::Uri::fileUriFromPath(main_file.genericString());

		compiler.addWorkspace(root_uri);

		auto cache_path = files.cachePath(main_file);

		// Opening swaps the module over to the editor's buffer.
		compiler.openDocument(file_uri, "duckling", 1, "fun main() -> i64 = { return 0; } // opened\n");

		assertTrue(cache_path.exists(), "The cache twin must exist after didOpen");
		// Once open, the module is keyed by the cache path, not by the file behind it.
		auto opened_backing = backingFileOf(cache_path);
		assertTrue(
			opened_backing.getFilePath() == cache_path,
			"The module must be backed by the cache twin after didOpen"
		);
		assertTrue(
			opened_backing.getContent().view().stringView()
				== "fun main() -> i64 = { return 0; } // opened\n",
			"The compiler must see the buffer, not the file behind it"
		);

		// Changing is visible to the compiler without touching what is behind the buffer.
		assertTrue(
			compiler
			    .updateDocument(
					file_uri,
					2,
					{ lsp::TextDocumentContentChangeWholeDocument{
						.text = "fun main() -> i64 = { return 0; } // changed\n" } }
				)
			    .isOk(),
			"The change must be applied"
		);
		assertTrue(
			backingFileOf(cache_path).getContent().view().stringView()
				== "fun main() -> i64 = { return 0; } // changed\n",
			"The compiler must see the changed buffer"
		);
		assertTrue(
			fs::File(main_file).getContent().view().stringView() == disk_text,
			"An unsaved edit must never reach what is behind the buffer"
		);

		// Closing swaps back and drops the twin, leaving the file behind it untouched.
		assertTrue(compiler.closeDocument(file_uri).isOk(), "The document must close");
		assertTrue(!cache_path.exists(), "The cache twin must be gone after didClose");

		auto closed_backing = backingFileOf(main_file);
		assertTrue(
			closed_backing.getFilePath() == main_file,
			"The module must be backed by the original file after didClose"
		);
		assertTrue(
			closed_backing.getContent().view().stringView() == disk_text,
			"The original content must be back after didClose"
		);

		std::filesystem::remove_all(workspace_path.getPath());
	}

	/**
	 * @brief Runs a scripted session against a server wired to the real compiler.
	 */
	std::string runCompilerSession(const std::vector<std::string>& messages) {
		duck_ls_test::StringStream stream(framed(messages));

		lsp::ServerEndpoint    endpoint(stream);
		duck_ls::ServerSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache    files;
		duck_ls::Compiler      compiler{ base::Ref<duck_ls::ServerSession>(&session),
		                                 base::Ref<duck_ls::FilesCache>(&files) };

		session.registerHandlers(compiler);
		endpoint.runMessageLoop();

		return stream.written();
	}

	void diagnosticsTest() {
		auto workspace_path = fs::FilePath(std::filesystem::current_path()).join("lsp_diag_ws");
		std::filesystem::remove_all(workspace_path.getPath());

		auto workspace = fs::FileManager::createPhysicalFolder(workspace_path, true);
		auto package   = workspace.createSubDirectory("pkg");
		auto main      = package.createSubFile("fun main() -> i64 = { return 0; }\n", "pkg.dk");

		auto root_uri = lsp::Uri::fileUriFromPath(workspace.getFilePath().genericString());
		auto file_uri = lsp::Uri::fileUriFromPath(main.getFilePath().genericString());

		auto initialize
			= R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":")"
		    + root_uri.toString() + R"(","capabilities":{}}})";
		auto did_open
			= R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")"
		    + file_uri.toString()
		    + R"(","languageId":"duckling","version":1,"text":"fun main() -> i64 = { this is not duckling }\n"}}})";
		auto did_change
			= R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":")"
		    + file_uri.toString()
		    + R"(","version":2},"contentChanges":[{"text":"fun main() -> i64 = { return 0; }\n"}]}})";

		auto written = compact(runCompilerSession(
			{
				initialize,
				R"({"jsonrpc":"2.0","method":"initialized","params":{}})",
				did_open,
				did_change,
				R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})",
				R"({"jsonrpc":"2.0","method":"exit"})",
			}
		));

		std::filesystem::remove_all(workspace_path.getPath());

		assertTrue(
			written.find(R"("method":"textDocument/publishDiagnostics")") != std::string::npos,
			"Diagnostics must be published: " + written
		);
		assertTrue(
			written.find(R"("uri":")" + file_uri.toString() + R"(")") != std::string::npos,
			"Diagnostics must be named by the URI the client used: " + written
		);
		assertTrue(
			written.find(R"("diagnostics":[])") != std::string::npos,
			"Fixing the file must publish an empty array: " + written
		);
	}

	void virtualWorkspaceTest() {
		auto vfs = fs::VFS::getInstance();

		auto workspace = fs::FileManager::createVirtualFolder(
			fs::FilePath(vfs->getRootPath()).join("vws"), true
		);
		auto package = workspace.createSubDirectory("pkg");
		auto main    = package.createSubFile("fun main() -> i64 = { return 0; }\n", "pkg.dk");

		duck_ls_test::StringStream stream("");
		lsp::ServerEndpoint        endpoint(stream);
		CollectingSession          session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache        files{ vfs };
		duck_ls::Compiler   compiler{ base::Ref<duck_ls::ServerSession>(&session),
		                              base::Ref<duck_ls::FilesCache>(&files) };

		auto root_uri = lsp::Uri::fileUriFromPath(workspace.getFilePath().toPhysicalPath().genericString());
		auto file_uri = lsp::Uri::fileUriFromPath(main.getFilePath().toPhysicalPath().genericString());

		compiler.addWorkspace(root_uri);
		compiler.openDocument(file_uri, "duckling", 1, "fun main() -> i64 = { this is not duckling }\n");
		compiler.publishDiagnostics(file_uri);

		assertTrue(
			session.pushed.contains(file_uri), "The opened document must get diagnostics"
		);
		assertTrue(
			!session.pushed.at(file_uri).empty(), "A broken buffer must produce diagnostics"
		);

		compiler.updateDocument(
			file_uri,
			2,
			{ lsp::TextDocumentContentChangeWholeDocument{
				.text = "fun main() -> i64 = { return 0; }\n" } }
		);
		compiler.publishDiagnostics(file_uri);

		assertTrue(
			session.pushed.at(file_uri).empty(), "Fixing the buffer must clear the diagnostics"
		);

		compiler.closeDocument(file_uri);
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
