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
#include <unordered_map>
#include <vector>

namespace {
	/**
	 * @brief A session that keeps what it was asked to push instead of writing to a client.
	 *
	 * The endpoint it is built on is never used: `pushDiagnostics` is the only thing the
	 * compiler calls, and it is overridden here.
	 */
	struct CollectingSession final: public duck_ls::ServerSession {
		using ServerSession::ServerSession;

		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> pushed;

		void pushDiagnostics(const lsp::Uri& uri, const std::vector<lsp::Diagnostic>& diagnostics)
			override {
			pushed[uri] = diagnostics;
		}
	};

	/**
	 * @brief A compiler wired to a collecting session, with the endpoint it never talks to.
	 *
	 * Everything is held together so that a test can name one object and get a working compiler.
	 */
	struct CompilerUnderTest final {
		explicit CompilerUnderTest(base::Optional<base::Ref<fs::VFS>> source_vfs = {}):
			  files(source_vfs),
			  compiler{ &session, &files } {}

		duck_ls_test::StringStream stream{ "" };
		lsp::ServerEndpoint        endpoint{ stream };
		CollectingSession          session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache        files;
		duck_ls::Compiler          compiler;
	};
}

class LspServerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspServerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
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
	/**
	 * @brief A change replacing the whole buffer with `text`.
	 */
	lsp::TextDocumentContentChangeEvent wholeDocument(std::string text) {
		return lsp::TextDocumentContentChangeWholeDocument{ .text = std::move(text) };
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

		const auto        main_file = main.getFilePath();
		const std::string disk_text{ fs::File(main_file).getContent().view().stringView() };

		CompilerUnderTest under_test;

		auto root_uri = lsp::Uri::fileUriFromPath(workspace.getFilePath().genericString());
		auto file_uri = lsp::Uri::fileUriFromPath(main_file.genericString());

		under_test.compiler.addWorkspace(root_uri);

		auto cache_path = under_test.files.cachePath(main_file);

		// Opening swaps the module over to the editor's buffer.
		under_test.compiler.openDocument(
			file_uri, "duckling", 1, "fun main() -> i64 = { return 0; } // opened\n"
		);

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
			under_test.compiler
				.updateDocument(
					file_uri, 2, { wholeDocument("fun main() -> i64 = { return 0; } // changed\n") }
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
		assertTrue(under_test.compiler.closeDocument(file_uri).isOk(), "The document must close");
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
	 * @brief Opens a broken buffer over `file_uri`, then fixes it, asserting what is published.
	 *
	 * The same scenario answers for a package on disk and for one in a virtual filesystem, so
	 * the two differ only in how the tree was built.
	 */
	void runDiagnosticsCycle(
		CompilerUnderTest& under_test, const lsp::Uri& root_uri, const lsp::Uri& file_uri
	) {
		under_test.compiler.addWorkspace(root_uri);

		under_test.compiler.openDocument(
			file_uri, "duckling", 1, "fun main() -> i64 = { this is not duckling }\n"
		);
		under_test.compiler.publishDiagnostics(file_uri);

		assertTrue(
			!under_test.session.pushed.at(file_uri).empty(),
			"A broken buffer must produce diagnostics"
		);

		assertTrue(
			under_test.compiler
				.updateDocument(
					file_uri, 2, { wholeDocument("fun main() -> i64 = { return 0; }\n") }
				)
				.isOk(),
			"The fix must be applied"
		);
		under_test.compiler.publishDiagnostics(file_uri);

		assertTrue(
			under_test.session.pushed.at(file_uri).empty(),
			"Fixing the buffer must publish an empty array so the client clears it"
		);

		assertTrue(under_test.compiler.closeDocument(file_uri).isOk(), "The document must close");
	}

	void diagnosticsTest() {
		auto workspace_path = fs::FilePath(std::filesystem::current_path()).join("lsp_diag_ws");
		std::filesystem::remove_all(workspace_path.getPath());

		auto workspace = fs::FileManager::createPhysicalFolder(workspace_path, true);
		auto package   = workspace.createSubDirectory("pkg");
		auto main      = package.createSubFile("fun main() -> i64 = { return 0; }\n", "pkg.dk");

		CompilerUnderTest under_test;

		runDiagnosticsCycle(
			under_test,
			lsp::Uri::fileUriFromPath(workspace.getFilePath().genericString()),
			lsp::Uri::fileUriFromPath(main.getFilePath().genericString())
		);

		std::filesystem::remove_all(workspace_path.getPath());
	}

	void virtualWorkspaceTest() {
		auto vfs = fs::VFS::getInstance();

		auto workspace = fs::FileManager::createVirtualFolder(
			fs::FilePath(vfs->getRootPath()).join("vws"), true
		);
		auto package = workspace.createSubDirectory("pkg");
		auto main    = package.createSubFile("fun main() -> i64 = { return 0; }\n", "pkg.dk");

		CompilerUnderTest under_test{ vfs };

		// The client never sees the virtual filesystem: it names files by the path the tree
		// would have on disk, and the cache resolves them back into the VFS.
		runDiagnosticsCycle(
			under_test,
			lsp::Uri::fileUriFromPath(workspace.getFilePath().toPhysicalPath().genericString()),
			lsp::Uri::fileUriFromPath(main.getFilePath().toPhysicalPath().genericString())
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
