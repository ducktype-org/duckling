// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "test_utils.hpp"

#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>
#include <version/version.hpp>

#include <base/types/ok_bad.hpp>

#include <tester/tester.hpp>

#include <string>
#include <vector>

namespace {
	/**
	 * @brief Records what the handlers ask of the compiler instead of compiling anything.
	 *
	 * Every method is overridden, so no call ever reaches the real implementation and the
	 * dependencies the base needs are never touched.
	 */
	class RecordingCompiler final: public duck_ls::Compiler {
	public:
		using Compiler::Compiler;

		struct Call final {
			std::string name{};
			std::string uri{};
			/// For a change notification, how many changes it carried.
			usize change_count = 0;
			/// The version the call carried, for the notifications that have one.
			i32         version = 0;
			std::string language_id{};
			std::string text{};
		};

		std::vector<Call> calls;
		/// What the two fallible operations report back, so that the handler's reaction to a
		/// rejected change can be driven from a test.
		base::OkBad update_result = base::OK;
		base::OkBad close_result  = base::OK;
		/// What a semantic tokens request is answered with.
		base::Optional<lsp::SemanticTokens> semantic_tokens_result{};

		void addWorkspace(const lsp::Uri& root) override {
			calls.push_back(Call{ .name = "addWorkspace", .uri = root.toString() });
		}

		void openDocument(
			const lsp::Uri& uri, std::string_view language_id, i32 version, std::string_view text
		) override {
			calls.push_back(Call{ .name        = "openDocument",
			                      .uri         = uri.toString(),
			                      .version     = version,
			                      .language_id = std::string(language_id),
			                      .text        = std::string(text) });
		}

		base::OkBad updateDocument(
			const lsp::Uri&                                        uri,
			i32                                                    version,
			const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
		) override {
			calls.push_back(Call{ .name         = "updateDocument",
			                      .uri          = uri.toString(),
			                      .change_count = changes.size(),
			                      .version      = version });
			return update_result;
		}

		base::OkBad closeDocument(const lsp::Uri& uri) override {
			calls.push_back(Call{ .name = "closeDocument", .uri = uri.toString() });
			return close_result;
		}

		void fileCreatedOrDeletedOnDisk(const lsp::Uri& uri) override {
			calls.push_back(Call{ .name = "fileCreatedOrDeletedOnDisk", .uri = uri.toString() });
		}

		void publishDiagnostics(const base::Optional<lsp::Uri>& queried_uri_opt) override {
			calls.push_back(Call{
				.name = "publishDiagnostics",
				.uri
				= queried_uri_opt.has_value() ? queried_uri_opt.value().toString() : std::string{},
			});
		}

		base::Optional<lsp::SemanticTokens> semanticTokens(const lsp::Uri& uri) override {
			calls.push_back(Call{ .name = "semanticTokens", .uri = uri.toString() });
			return semantic_tokens_result;
		}
	};

	struct SessionRun final {
		std::string                          written;
		std::vector<RecordingCompiler::Call> calls;

		[[nodiscard]] std::vector<std::string> names() const {
			std::vector<std::string> result;
			result.reserve(calls.size());
			for (const auto& call: calls) result.push_back(call.name);
			return result;
		}
	};

	/**
	 * @brief Drives a session over a scripted conversation and reports what it asked of the
	 * compiler.
	 */
	SessionRun runSession(
		const std::vector<std::string>&     messages,
		base::OkBad                         update_result          = base::OK,
		base::OkBad                         close_result           = base::OK,
		base::Optional<lsp::SemanticTokens> semantic_tokens_result = {}
	) {
		std::string input;
		for (const auto& message: messages) input += duck_ls_test::frame(message);

		duck_ls_test::StringStream stream(input);

		lsp::ServerEndpoint    endpoint(stream);
		duck_ls::ServerSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache    files;
		RecordingCompiler      compiler{ base::Ref<duck_ls::ServerSession>(&session),
                                    base::Ref<duck_ls::FilesCache>(&files) };

		compiler.update_result          = update_result;
		compiler.close_result           = close_result;
		compiler.semantic_tokens_result = std::move(semantic_tokens_result);

		session.registerHandlers(compiler);
		endpoint.runMessageLoop();

		return { .written = stream.written(), .calls = compiler.calls };
	}

	const std::string INITIALIZE
		= R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})";
	const std::string INITIALIZED = R"({"jsonrpc":"2.0","method":"initialized","params":{}})";
	const std::string SHUTDOWN    = R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})";
	const std::string EXIT        = R"({"jsonrpc":"2.0","method":"exit"})";

	/**
	 * @brief Wraps `messages` in the lifecycle every notification handler needs around it.
	 */
	std::vector<std::string> session(const std::vector<std::string>& messages) {
		std::vector<std::string> result{ INITIALIZE, INITIALIZED };
		result.insert(result.end(), messages.begin(), messages.end());
		result.push_back(SHUTDOWN);
		result.push_back(EXIT);
		return result;
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

class LspSessionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspSessionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(initializeCapabilitiesTest);
		TESTER_ADD_TEST(initializeWorkspaceRootsTest);
		TESTER_ADD_TEST(didOpenTest);
		TESTER_ADD_TEST(didChangeTest);
		TESTER_ADD_TEST(didCloseTest);
		TESTER_ADD_TEST(didChangeWatchedFilesTest);
		TESTER_ADD_TEST(didChangeWatchedFilesWithNothingElseTest);
		TESTER_ADD_TEST(unsupportedUriIsNotFatalTest);
		TESTER_ADD_TEST(initializeSemanticTokensCapabilityTest);
		TESTER_ADD_TEST(semanticTokensFullTest);
		TESTER_ADD_TEST(semanticTokensFullUnknownDocumentTest);
	}

private:
	/**
	 * @brief Assert that called the stub Compiler methods from the run are the same as expected.
	 */
	void assertMethodCalls(
		const SessionRun& run, const std::vector<std::string>& expected, const std::string& what
	) {
		auto actual = run.names();

		std::string rendered;
		for (const auto& name: actual) rendered += name + " ";

		assertTrue(
			actual.size() == expected.size(),
			what + ": wrong number of compiler calls, got [ " + rendered + "]"
		);
		for (usize i = 0; i < expected.size() && i < actual.size(); ++i)
			assertTrue(
				actual[i] == expected[i],
				what + ": call " + std::to_string(i) + " must be " + expected[i] + ", was "
					+ actual[i]
			);
	}

	void initializeCapabilitiesTest() {
		auto written = compact(runSession({ INITIALIZE, INITIALIZED, SHUTDOWN, EXIT }).written);

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

		// Asking the same source the server does, rather than repeating a literal that goes
		// stale on the next release.
		const auto expected_version = compact(
			R"X("version":")X" + std::string(version::semver()) + " ("
			+ std::string(version::commitHash()) + R"X()")X"
		);
		assertTrue(
			written.find(expected_version) != std::string::npos,
			"initialize must report the build version, expected " + expected_version
				+ " in: " + written
		);
	}

	void initializeWorkspaceRootsTest() {
		auto run = runSession({
			R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,)"
			R"("rootUri":"file:///ws/root","capabilities":{},)"
			R"("workspaceFolders":[{"uri":"file:///ws/a","name":"a"},{"uri":"file:///ws/b","name":"b"}]}})",
			INITIALIZED,
			SHUTDOWN,
			EXIT,
		});

		assertMethodCalls(run, { "addWorkspace", "addWorkspace", "addWorkspace" }, "initialize");

		// The folders come first, in the order the client listed them, and the deprecated
		// rootUri is forwarded after them.
		assertTrue(run.calls[0].uri == "file:///ws/a", "First root must be the first folder");
		assertTrue(run.calls[1].uri == "file:///ws/b", "Second root must be the second folder");
		assertTrue(run.calls[2].uri == "file:///ws/root", "Last root must be rootUri");
	}

	void didOpenTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":)"
			R"({"uri":"file:///ws/a.dk","languageId":"duckling","version":7,"text":"abc"}}})",
		}));

		assertMethodCalls(run, { "openDocument", "publishDiagnostics" }, "didOpen");

		assertTrue(run.calls[0].uri == "file:///ws/a.dk", "The open call must name the document");
		assertTrue(run.calls[0].version == 7, "The open call must carry the client's version");
		assertTrue(run.calls[0].language_id == "duckling", "The language id must be forwarded");
		assertTrue(run.calls[0].text == "abc", "The initial text must be forwarded");
		assertTrue(
			run.calls[1].uri == "file:///ws/a.dk", "Diagnostics must be published for the document"
		);
	}

	void didChangeTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":)"
			R"({"uri":"file:///ws/a.dk","version":2},"contentChanges":[)"
			R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"text":"A"},)"
			R"({"text":"whole"}]}})",
		}));

		assertMethodCalls(run, { "updateDocument", "publishDiagnostics" }, "didChange");

		assertTrue(run.calls[0].version == 2, "The change must carry the client's version");
		assertTrue(run.calls[0].change_count == 2, "Both edits must reach the compiler");
	}

	void didCloseTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":)"
			R"({"uri":"file:///ws/a.dk"}}})",
		}));

		assertMethodCalls(run, { "closeDocument", "publishDiagnostics" }, "didClose");
		assertTrue(run.calls[0].uri == "file:///ws/a.dk", "The close call must name the document");
	}

	void didChangeWatchedFilesTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[)"
			R"({"uri":"file:///ws/created.dk","type":1},)"
			R"({"uri":"file:///ws/changed.dk","type":2},)"
			R"({"uri":"file:///ws/deleted.dk","type":3}]}})",
		}));

		// A file whose content changed is already covered by the buffer or by the next open, so
		// only appearing and disappearing files reach the compiler. The batch is published once
		// at the end: a deleted file has no module to be asked about, and one batch can reach
		// several packages.
		assertMethodCalls(
			run,
			{ "fileCreatedOrDeletedOnDisk", "fileCreatedOrDeletedOnDisk", "publishDiagnostics" },
			"didChangeWatchedFiles"
		);

		assertTrue(run.calls[0].uri == "file:///ws/created.dk", "The created file must be first");
		assertTrue(run.calls[1].uri == "file:///ws/deleted.dk", "The deleted file must be second");
		assertTrue(
			run.calls[2].uri.empty(),
			"The batch must be published for every tracked package, not for one file"
		);
	}

	void didChangeWatchedFilesWithNothingElseTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[)"
			R"({"uri":"file:///ws/changed.dk","type":2}]}})",
		}));

		// Nothing appeared or disappeared, so there is nothing to recompile and nothing to say.
		assertMethodCalls(run, {}, "didChangeWatchedFiles with only a content change");
	}

	void unsupportedUriIsNotFatalTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":)"
			R"({"uri":"untitled:Untitled-1","languageId":"duckling","version":1,"text":"abc"}}})",
			R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":)"
			R"({"uri":"file:///ws/a.dk","languageId":"duckling","version":1,"text":"abc"}}})",
		}));

		// Naming what the server cannot open is the compiler's decision, not the session's, so
		// the notification is forwarded like any other and the connection survives it.
		assertMethodCalls(
			run,
			{ "openDocument", "publishDiagnostics", "openDocument", "publishDiagnostics" },
			"unsupported URI"
		);
		assertTrue(
			run.calls[0].uri == "untitled:Untitled-1",
			"The URI must be forwarded as the client spelled it"
		);
	}

	void initializeSemanticTokensCapabilityTest() {
		auto written = compact(runSession({ INITIALIZE, INITIALIZED, SHUTDOWN, EXIT }).written);

		assertTrue(
			written.find(R"("semanticTokensProvider":{)") != std::string::npos,
			"initialize must advertise semantic tokens: " + written
		);
		assertTrue(
			written.find(R"("tokenTypes":["namespace","type","class")") != std::string::npos,
			"initialize must advertise the standard token types in order: " + written
		);
		assertTrue(
			written.find(R"("full":true)") != std::string::npos,
			"initialize must advertise full semantic tokens: " + written
		);
	}

	void semanticTokensFullTest() {
		auto run = runSession(
			session({
				R"({"jsonrpc":"2.0","id":3,"method":"textDocument/semanticTokens/full","params":)"
				R"({"textDocument":{"uri":"file:///ws/a.dk"}}})",
			}),
			base::OK,
			base::OK,
			lsp::SemanticTokens{ .data = { 0, 0, 3, 15, 0 } }
		);

		assertMethodCalls(run, { "semanticTokens" }, "semanticTokens/full");
		assertTrue(run.calls[0].uri == "file:///ws/a.dk", "The request must name the document");

		auto written = compact(run.written);
		assertTrue(
			written.find(R"("id":3,"result":{"data":[0,0,3,15,0]})") != std::string::npos,
			"The tokens must be the response: " + written
		);
	}

	void semanticTokensFullUnknownDocumentTest() {
		auto run = runSession(session({
			R"({"jsonrpc":"2.0","id":3,"method":"textDocument/semanticTokens/full","params":)"
			R"({"textDocument":{"uri":"file:///ws/a.dk"}}})",
		}));

		assertMethodCalls(run, { "semanticTokens" }, "semanticTokens/full without tokens");
		auto written = compact(run.written);
		assertTrue(
			written.find(R"("id":3,"result":null)") != std::string::npos,
			"A document without tokens must be answered with null: " + written
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
