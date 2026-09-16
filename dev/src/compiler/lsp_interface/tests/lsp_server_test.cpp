#include "string_stream.hpp"
#include "vfs_workspace.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>
#include <lsp_interface/compiler_files_management.hpp>
#include <lsp_interface/diagnostics.hpp>
#include <lsp_interface/handlers.hpp>
#include <lsp_interface/uri_conversion.hpp>

#include <diagnostic/logger.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

#include <vector>

namespace {
	/**
	 * @brief Records every call the handlers make instead of touching the compiler.
	 */
	class RecordingFilesManagement final: public duck_ls::IFilesManagement {
	public:
		struct Call final {
			std::string  name;
			fs::FilePath path;
			std::string  text;
		};

		std::vector<Call> calls;

		void addWorkspace(const fs::FilePath& root) override {
			calls.push_back({ "addWorkspace", root, "" });
		}

		void openDocument(const fs::FilePath& path, std::string_view text) override {
			calls.push_back({ "openDocument", path, std::string(text) });
		}

		void updateDocument(const fs::FilePath& path, std::string_view text) override {
			calls.push_back({ "updateDocument", path, std::string(text) });
		}

		void closeDocument(const fs::FilePath& path) override {
			calls.push_back({ "closeDocument", path, "" });
		}

		void fileCreatedOrDeletedOnDisk(const fs::FilePath& path) override {
			calls.push_back({ "fileCreatedOrDeletedOnDisk", path, "" });
		}
	};

	/**
	 * @brief Feeds a scripted session to a server driven entirely in process.
	 */
	std::string runSession(
		const std::vector<std::string>&              messages,
		std::vector<RecordingFilesManagement::Call>* out_calls = nullptr
	) {
		std::string input;
		for (const auto& message: messages) input += duck_ls_test::frame(message);

		duck_ls_test::StringStream stream(input);

		auto files     = base::makeBox<RecordingFilesManagement>();
		auto files_ref = base::Ref<RecordingFilesManagement>(files.get());

		duck_ls::ServerSession session;
		session.setFiles(std::move(files));
		lsp::ServerEndpoint endpoint(stream);
		duck_ls::registerHandlers(endpoint, session);
		endpoint.runMessageLoop();

		if (out_calls != nullptr) *out_calls = files_ref->calls;

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
		TESTER_ADD_TEST(uriConversionTest);
		TESTER_ADD_TEST(lifecycleTest);
		TESTER_ADD_TEST(requestBeforeInitializeTest);
		TESTER_ADD_TEST(incrementalSyncTest);
		TESTER_ADD_TEST(multiByteSyncTest);
		TESTER_ADD_TEST(outOfOrderVersionTest);
		TESTER_ADD_TEST(vfsWorkspaceSwapTest);
		TESTER_ADD_TEST(temporaryDirectorySwapTest);
		TESTER_ADD_TEST(diagnosticsTest);
	}

protected:
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
		query::setTrackReverseGraph(true);
		compiler::frontend::use_module_modifier_remove = true;
	}

private:
	void uriConversionTest() {
		duck_ls::OpenDocuments documents;

		auto roundTrip = [&](const std::string& physical) {
			auto uri  = lsp::Uri::fileUriFromPath(physical);
			auto back = duck_ls::toPhysicalPath(uri);
			ASSERT_HAS_VALUE(back, "A file URI must convert back to a path: " + physical);
			assertTrue(
				back->genericString() == physical,
				"Round trip changed the path: " + physical + " -> " + back->genericString()
			);
		};

		roundTrip("/ws/duck ls/a file.dk");
		roundTrip("/ws/duck#hash/a+plus.dk");
		roundTrip("/ws/duck~tilde/zażółć.dk");
		roundTrip("/ws/duck%percent/a.dk");

		// Non-file schemes are rejected instead of reaching path handling.
		ASSERT_NO_VALUE(
			duck_ls::toPhysicalPath(lsp::Uri::parse("untitled:Untitled-1")),
			"untitled: must not convert to a path"
		);
		ASSERT_NO_VALUE(
			duck_ls::toPhysicalPath(lsp::Uri::parse("https://example.com/a.dk")),
			"https: must not convert to a path"
		);
		ASSERT_NO_VALUE(duck_ls::toPhysicalPath(lsp::Uri()), "An invalid URI must not convert");

		// Dot segments normalize.
		auto dotted = duck_ls::toPhysicalPath(lsp::Uri::parse("file:///ws/a/./b/../c.dk"));
		ASSERT_HAS_VALUE(dotted, "A dotted file URI must convert");
		assertTrue(dotted->genericString() == "/ws/a/c.dk", "Dot segments must normalize");

		// A path with no open document encodes from scratch.
		fs::FilePath physical("/ws/duck ls/closed.dk");
		assertTrue(
			duck_ls::toDocumentUri(physical, documents).toString()
				== lsp::Uri::fileUriFromPath("/ws/duck ls/closed.dk").toString(),
			"A closed file must be named by its own encoded URI"
		);

		// An open document is answered in the client's own spelling.
		auto client_uri = lsp::Uri::parse("file:///ws/duck%20ls/open.dk");
		documents.insert(
			duck_ls::OpenDocument{
				.uri           = client_uri,
				.physical_path = fs::FilePath("/ws/duck ls/open.dk"),
				.language_id   = "duckling",
				.text          = "",
				.line_starts   = {},
				.is_ascii      = true,
			}
		);

		assertTrue(
			duck_ls::toDocumentUri(fs::FilePath("/ws/duck ls/open.dk"), documents).toString()
				== client_uri.toString(),
			"An open file must be named by the client's own spelling"
		);
		assertTrue(
			duck_ls::toDocumentUri(fs::FilePath("/ws/duck ls/open.dk").toVirtualPath(), documents)
					.toString()
				== client_uri.toString(),
			"A virtual path must resolve to the client's stored URI"
		);
	}

	void lifecycleTest() {
		auto written = compact(runSession(
			{
				R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})",
				R"({"jsonrpc":"2.0","method":"initialized","params":{}})",
				R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})",
				R"({"jsonrpc":"2.0","method":"exit"})",
			}
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
			}
		));

		assertTrue(
			written.find("-32002") != std::string::npos,
			"A request before initialize must be rejected with ServerNotInitialized: " + written
		);
	}

	/**
	 * @brief Runs an initialize / didOpen / didChange... / didClose session over one document.
	 */
	std::vector<RecordingFilesManagement::Call> runDocumentSession(
		const std::string& open_text, const std::vector<std::string>& change_params
	) {
		std::vector<std::string> messages{
			R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})",
			R"({"jsonrpc":"2.0","method":"initialized","params":{}})",
			R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///ws/a.dk","languageId":"duckling","version":1,"text":)"
				+ open_text + R"(}}})",
		};

		for (const auto& params: change_params)
			messages.push_back(
				R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":)" + params + "}"
			);

		messages.push_back(
			R"({"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"file:///ws/a.dk"}}})"
		);
		messages.push_back(R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})");
		messages.push_back(R"({"jsonrpc":"2.0","method":"exit"})");

		std::vector<RecordingFilesManagement::Call> calls;
		runSession(messages, &calls);
		return calls;
	}

	void incrementalSyncTest() {
		auto calls = runDocumentSession(
			R"("line one\nline two\n")",
			{
				R"({"textDocument":{"uri":"file:///ws/a.dk","version":2},"contentChanges":[{"range":{"start":{"line":0,"character":5},"end":{"line":0,"character":8}},"text":"ONE"}]})",
				R"({"textDocument":{"uri":"file:///ws/a.dk","version":3},"contentChanges":[{"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":4}},"text":"LINE"},{"range":{"start":{"line":2,"character":0},"end":{"line":2,"character":0}},"text":"tail"}]})",
			}
		);

		assertTrue(calls.size() == 4, "Expected open, two updates and a close");
		assertTrue(calls[0].name == "openDocument", "First call must be openDocument");
		assertTrue(
			calls[0].path == fs::FilePath("/ws/a.dk"), "openDocument must use the physical path"
		);
		assertTrue(calls[0].text == "line one\nline two\n", "Wrong opened text");
		assertTrue(calls[1].text == "line ONE\nline two\n", "Wrong text after edit 1");
		assertTrue(calls[2].text == "line ONE\nLINE two\ntail", "Wrong text after edit 2");
		assertTrue(calls[3].name == "closeDocument", "Last call must be closeDocument");
	}

	void multiByteSyncTest() {
		// "źó" is two code points of two bytes each, so a UTF-16 column is not a byte offset.
		auto calls = runDocumentSession(
			R"("źód\n")",
			{ R"({"textDocument":{"uri":"file:///ws/a.dk","version":2},"contentChanges":[{"range":{"start":{"line":0,"character":2},"end":{"line":0,"character":3}},"text":"D"}]})" }
		);

		assertTrue(calls.size() == 3, "Expected open, one update and a close");
		assertTrue(calls[1].text == "źóD\n", "Wrong text after a multi byte edit");
	}

	void outOfOrderVersionTest() {
		auto calls = runDocumentSession(
			R"("abc")",
			{
				R"({"textDocument":{"uri":"file:///ws/a.dk","version":2},"contentChanges":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"text":"A"}]})",
				R"({"textDocument":{"uri":"file:///ws/a.dk","version":2},"contentChanges":[{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":2}},"text":"B"}]})",
			}
		);

		assertTrue(calls.size() == 3, "A repeated version must not reach the compiler");
		assertTrue(calls[1].text == "Abc", "Only the in order change may apply");
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
	 * @brief Drives one open / change / close cycle over a package rooted at `package_root`.
	 */
	void runSwapCycle(const fs::FilePath& workspace_root, const fs::FilePath& main_file) {
		const std::string disk_text{ fs::File(main_file).getContent().view().stringView() };

		duck_ls::ServerSession session;
		session.setFiles(base::makeBox<duck_ls::CompilerFilesManagement>(base::Ref(&session)));
		session.filesManagement().addWorkspace(workspace_root);

		// Opening swaps the module over to the editor's buffer.
		session.filesManagement().openDocument(
			main_file, "fun main() -> i64 = { return 0; } // opened\n"
		);

		assertTrue(session.cachePath(main_file).exists(), "The cache twin must exist after didOpen");
		// Once open, the module is keyed by the cache twin, not by the file behind it.
		auto opened_backing = backingFileOf(session.cachePath(main_file));
		assertTrue(
			opened_backing.getFilePath() == session.cachePath(main_file),
			"The module must be backed by the cache twin after didOpen"
		);
		assertTrue(
			opened_backing.getContent().view().stringView()
				== "fun main() -> i64 = { return 0; } // opened\n",
			"The compiler must see the buffer, not the file behind it"
		);

		// Changing is visible to the compiler without touching what is behind the buffer.
		session.filesManagement().updateDocument(
			main_file, "fun main() -> i64 = { return 0; } // changed\n"
		);
		assertTrue(
			backingFileOf(session.cachePath(main_file)).getContent().view().stringView()
				== "fun main() -> i64 = { return 0; } // changed\n",
			"The compiler must see the changed buffer"
		);
		assertTrue(
			fs::File(main_file).getContent().view().stringView() == disk_text,
			"An unsaved edit must never reach what is behind the buffer"
		);

		// Closing swaps back and drops the twin, leaving the file behind it untouched.
		session.filesManagement().closeDocument(main_file);
		assertTrue(
			!session.cachePath(main_file).exists(), "The cache twin must be gone after didClose"
		);

		auto closed_backing = backingFileOf(main_file);
		assertTrue(
			closed_backing.getFilePath() == main_file,
			"The module must be backed by the original file after didClose"
		);
		assertTrue(
			closed_backing.getContent().view().stringView() == disk_text,
			"The original content must be back after didClose"
		);
	}

	void vfsWorkspaceSwapTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_swap_ws");
		workspace.addDirectory("pkg");
		auto main_file = workspace.addFile("pkg/pkg.dk", "fun main() -> i64 = { return 0; }\n");
		workspace.addFile("pkg/helper.dk", "fun helper() -> i64 = { return 1; }\n");

		runSwapCycle(workspace.root(), main_file);
	}

	void temporaryDirectorySwapTest() {
		// Covers the Physical -> Virtual registry key transition, which the VFS-as-disk setup
		// cannot reach because both sides are virtual there. The tree is built next to the test
		// binary rather than in the temp directory, because a temp path is its own PathType and
		// cannot be converted to a virtual one.
		auto workspace_path
			= fs::FilePath(std::filesystem::current_path()).join("lsp_swap_physical_ws");
		std::filesystem::remove_all(workspace_path.getPath());

		auto workspace = fs::FileManager::createPhysicalFolder(workspace_path, true);
		auto package   = workspace.createSubDirectory("pkg");
		auto main      = package.createSubFile("fun main() -> i64 = { return 0; }\n", "pkg.dk");
		auto helper = package.createSubFile("fun helper() -> i64 = { return 1; }\n", "helper.dk");
		(void) helper;

		runSwapCycle(workspace.getFilePath(), main.getFilePath());

		std::filesystem::remove_all(workspace_path.getPath());
	}

	/**
	 * @brief Runs a scripted session against a server wired to the real compiler.
	 */
	std::string runCompilerSession(const std::vector<std::string>& messages) {
		std::string input;
		for (const auto& message: messages) input += duck_ls_test::frame(message);

		duck_ls_test::StringStream stream(input);

		duck_ls::ServerSession session;
		session.setFiles(base::makeBox<duck_ls::CompilerFilesManagement>(base::Ref(&session)));

		lsp::ServerEndpoint endpoint(stream);
		duck_ls::registerHandlers(endpoint, session);
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
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
