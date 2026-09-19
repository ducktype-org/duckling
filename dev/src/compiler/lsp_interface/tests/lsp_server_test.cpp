#include "test_utils.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/global_logger.hpp>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>

#include "base/except/exceptions.hpp"

#include <diagnostic/logger.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

#include <string>

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

		/**
		 * @brief Whether the last publish for `uri` reported something.
		 */
		[[nodiscard]] bool hasErrors(const lsp::Uri& uri) const {
			auto it = pushed.find(uri);
			return it != pushed.end() && !it->second.empty();
		}

		/**
		 * @brief Whether `uri` was published with nothing to report.
		 *
		 * False for a URI that was never published at all: a file the server said nothing about
		 * is not a file the server found clean.
		 */
		[[nodiscard]] bool noErrors(const lsp::Uri& uri) const {
			auto it = pushed.find(uri);
			return it != pushed.end() && it->second.empty();
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

	std::string content(const fs::File& file) {
		return std::string(file.getContent().view().stringView());
	}
}

class LspServerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspServerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(openCloseFileTest);
		TESTER_ADD_TEST(diagnosticsTest);
		TESTER_ADD_TEST(multiFileWorkspaceTest);
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
	 * @brief The given file has it's own module tree loaded.
	 */
	void assertHasModuleLoadedFor(const fs::FilePath& path) {
		using namespace compiler::frontend;

		auto source_files = SourceFile::getSourceFilesFromPath(path);
		assertTrue(!source_files.empty(), "No source file is registered for " + path.string());

		auto module_id  = source_files.back()->getModule().illegalAccess().getID();
		auto module_ref = getModuleRef(module_id);
		assertEqual(
			getFileRef(module_ref->getMainSourceFile().illegalAccess().getID())
				->getFileIllegalAccess()
				.getFilePath(),
			path,
			"Invalid module file."
		);
	}

	/**
	 * @brief The given file has no modules loaded.
	 */
	void assertNoModuleLoadedFor(const fs::FilePath& path) {
		using namespace compiler::frontend;

		auto source_files = SourceFile::getSourceFilesFromPath(path);
		assertTrue(source_files.empty(), "A source file is still registered for " + path.string());
	}

	/**
	 * @brief Tests if the main module file swapping (between cache file and source file)
	 * works for the open and close operations,
	 * and the changes are reflected in the cache file.
	 */
	void openCloseFileTest() {
		const auto workspace_root = fs::File(fs::FilePath(path("modules")));
		const auto workspace_uri
			= lsp::Uri::fileUriFromPath(workspace_root.getFilePath().genericString());
		const auto main_file = fs::File(fs::FilePath(path("modules/swap_cycle/swap_cycle.dk")));
		const auto file_uri  = lsp::Uri::fileUriFromPath(main_file.getFilePath().genericString());

		CompilerUnderTest under_test;
		const std::string disk_original_content{ content(main_file) };

		under_test.compiler.addWorkspace(workspace_uri);
		auto cache_path = under_test.files.cachePath(main_file.getFilePath());

		constexpr auto OPENED_CONTENT  = "fun main() -> i64 = { return 0; } // opened\n";
		constexpr auto CHANGED_CONTENT = "fun main() -> i64 = { return 0; } // changed\n";

		under_test.compiler.openDocument(file_uri, "duckling", 1, OPENED_CONTENT);
		assertTrue(cache_path.exists(), "The cache twin must exist after didOpen");

		const auto cache_file = fs::File(cache_path);
		assertHasModuleLoadedFor(cache_path);
		assertNoModuleLoadedFor(main_file.getFilePath());

		assertTrue(
			under_test.compiler.updateDocument(file_uri, 2, { wholeDocument(CHANGED_CONTENT) })
				.isOk(),
			"The change must be applied"
		);
		assertTrue(content(cache_file) == CHANGED_CONTENT, "The cache file stale.");
		assertTrue(content(main_file) == disk_original_content, "The source file changed.");

		// Closing swaps back and drops the twin, leaving the file behind it untouched.
		assertTrue(under_test.compiler.closeDocument(file_uri).isOk(), "The document must close");
		assertTrue(!cache_path.exists(), "The cache twin must be gone after didClose");

		assertHasModuleLoadedFor(main_file.getFilePath());
		assertNoModuleLoadedFor(cache_path);
	}

	/**
	 * @brief Opens a broken buffer over `file_uri`, then fixes it, asserting what is published.
	 *
	 * The same scenario answers for a package on disk and for one in a virtual filesystem, so
	 * the two differ only in how the tree was built.
	 */
	void diagnosticsTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_diag_ws");
		workspace.add("pkg/pkg.dk", "fun main() -> i64 = { return 0; }\n");
		auto              root_uri = workspace.uriOf("");
		auto              main_uri = workspace.uriOf("pkg/pkg.dk");
		CompilerUnderTest under_test{ duck_ls_test::VfsWorkspace::vfs() };

		under_test.compiler.addWorkspace(root_uri);

		under_test.compiler.openDocument(
			main_uri, "duckling", 1, "const a = 0.1;\n fun foo() = { a + 1; }\n"
		);
		under_test.compiler.publishDiagnostics(main_uri);
		assertTrue(under_test.session.hasErrors(main_uri), "Must produce diagnostics");

		assertTrue(
			under_test.compiler
				.updateDocument(
					main_uri, 2, { wholeDocument("const a = 0.1;\n fun foo() = { a + 1.0; }\n") }
				)
				.isOk(),
			"The fix must be applied"
		);
		under_test.compiler.publishDiagnostics(main_uri);

		assertTrue(under_test.session.noErrors(main_uri), "Must be valid.");
		assertTrue(under_test.compiler.closeDocument(main_uri).isOk(), "The document must close");
	}

	void multiFileWorkspaceTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_multi_ws");
		workspace
			.add("pkg/pkg.dk", "import helper; fun foo() -> i64 = { return helper.helper(); }\n")
			.add("pkg/helper.dk", "fun helper() -> i64 = { return \"abc\"; }\n");

		CompilerUnderTest under_test{ duck_ls_test::VfsWorkspace::vfs() };

		const auto main_uri   = workspace.uriOf("pkg/pkg.dk");
		const auto helper_uri = workspace.uriOf("pkg/helper.dk");

		under_test.compiler.addWorkspace(workspace.uriOf());
		under_test.compiler.openDocument(
			main_uri,
			"duckling",
			1,
			"import helper; fun foo() -> i64 = { return helper.helper(); }\n"
		);
		under_test.compiler.publishDiagnostics(main_uri);

		assertTrue(under_test.session.hasErrors(helper_uri), "Invalid helper.dk");
		assertTrue(under_test.session.noErrors(main_uri), "Invalid main.dk");

		workspace.remove("pkg/helper.dk");
		under_test.compiler.fileCreatedOrDeletedOnDisk(helper_uri);
		under_test.compiler.publishDiagnostics(main_uri);

		assertTrue(under_test.session.hasErrors(main_uri), "Invalid main.dk");
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
