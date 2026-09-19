#include <lsp/error.h>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/server_session.hpp>
#include <version/version.hpp>

#include <base/str/str_utils.hpp>

#include <iostream>

namespace duck_ls {

	namespace {
		/**
		 * @brief What the server reports as its version when it greets the client.
		 */
		std::string serverVersion() {
			return base::strConcat(version::semver(), " (", version::commitHash(), ")");
		}

		void collectWorkspaceRoots(const lsp::InitializeParams& params, Compiler& compiler) {
			if (params.workspaceFolders.has_value() && !params.workspaceFolders->isNull())
				for (const auto& folder: params.workspaceFolders->value())
					compiler.addWorkspace(folder.uri);

			if (!params.rootUri.isNull()) compiler.addWorkspace(params.rootUri.value());
		}
	}

	ServerSession::ServerSession(base::Ref<lsp::ServerEndpoint> endpoint): endpoint(endpoint) {}

	void ServerSession::pushDiagnostics(
		const lsp::Uri& uri, const std::vector<lsp::Diagnostic>& diagnostics
	) {
		endpoint->textDocumentPublishDiagnostics({ .uri = uri, .diagnostics = diagnostics });
	}

	// NOLINTBEGIN
	void ServerSession::registerHandlers(Compiler& compiler) {
		endpoint
			->onInitialize([this, &compiler](const lsp::InitializeParams& params) -> auto {
				collectWorkspaceRoots(params, compiler);

				return lsp::InitializeResult{
						.capabilities = {
							.positionEncoding = position_encoding,
							.textDocumentSync = lsp::TextDocumentSyncOptions{
								.openClose = true,
								.change    = lsp::TextDocumentSyncKind::Incremental,
							},
						},
						.serverInfo = lsp::ServerInfo{
							.name    = "duck_ls",
							.version = serverVersion(),
						},
					};
			})
			.onInitialized([](auto&&) {})
			.onTextDocumentDidOpen([&compiler](lsp::DidOpenTextDocumentParams&& params) {
				const std::string language_id = params.textDocument.languageId;
				compiler.openDocument(
					params.textDocument.uri,
					language_id,
					params.textDocument.version,
					params.textDocument.text
				);

				compiler.publishDiagnostics(params.textDocument.uri);
			})
			.onTextDocumentDidChange([&compiler](lsp::DidChangeTextDocumentParams&& params) {
				compiler.updateDocument(
					params.textDocument.uri, params.textDocument.version, params.contentChanges
				);
				compiler.publishDiagnostics(params.textDocument.uri);
			})
			.onTextDocumentDidClose([&compiler](lsp::DidCloseTextDocumentParams&& params) {
				compiler.closeDocument(params.textDocument.uri);
				compiler.publishDiagnostics(params.textDocument.uri);
			})
			.onWorkspaceDidChangeWatchedFiles([&compiler](lsp::DidChangeWatchedFilesParams&& params
		                                      ) {
				for (const auto& change: params.changes) {
					if (change.type == lsp::FileChangeType::Changed) continue;
					compiler.fileCreatedOrDeletedOnDisk(change.uri);
					compiler.publishDiagnostics(change.uri);
				}
			})
			.onShutdown([]() -> lsp::ShutdownResult { return {}; })
			.onExit([]() {});
	}

	// NOLINTEND

}
