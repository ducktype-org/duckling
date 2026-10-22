// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <lsp/error.h>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/server_session.hpp>
#include <version/version.hpp>

#include <base/str/str_utils.hpp>

#include <cstdlib>
#include <iostream>

namespace duck_ls {

	namespace {
		/**
		 * @brief What the server reports as its version when it greets the client.
		 */
		std::string serverVersion() {
			return base::strConcat(version::semver(), " (", version::commitHash(), ")");
		}

		/**
		 * @brief Tells the client the server is going down, and why, before it does.
		 */
		void reportFatal(
			lsp::ServerEndpoint& endpoint, std::string_view method, std::string_view what
		) noexcept {
			try {
				std::cerr << "duck_ls: fatal while handling " << method << ":\n" << what << "\n";

				endpoint.windowLogMessage({
					.type = lsp::MessageType::Error,
					.message
					= base::strConcat("duck_ls failed while handling ", method, ":\n", what),
				});
				endpoint.windowShowMessage({
					.type    = lsp::MessageType::Error,
					.message = base::strConcat(
						"The Duckling language server failed while handling ",
						method,
						" and is shutting down. See the Duckling Language Server output for the "
						"details."
					),
				});
			} catch (...) {
				// The connection is the only way to reach the client, so when it is the thing
				// that broke there is nowhere left to report to.
			}
		}

		/**
		 * @brief Runs a handler, ending the process on anything it throws.
		 */
		template<class Operation>
		auto orAbort(lsp::ServerEndpoint& endpoint, std::string_view method, Operation&& operation)
			-> decltype(operation()) {
			try {
				return std::forward<Operation>(operation)();
			} catch (const lsp::RequestError&) { throw; } catch (const std::exception& e) {
				reportFatal(endpoint, method, e.what());
				std::abort();
			} catch (...) {
				reportFatal(endpoint, method, "an exception that is not a std::exception");
				std::abort();
			}
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

	// The handlers take their parameters by rvalue reference and hand them on without moving.
	// NOLINTBEGIN(cppcoreguidelines-rvalue-reference-param-not-moved)
	void ServerSession::registerHandlers(Compiler& compiler) {
		endpoint
			->onInitialize([this, &compiler](const lsp::InitializeParams& params) -> auto {
				return orAbort(*endpoint, "initialize", [&] {
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
				});
			})
			.onInitialized([](auto&&) {})
			.onTextDocumentDidOpen([this, &compiler](lsp::DidOpenTextDocumentParams&& params) {
				orAbort(*endpoint, "textDocument/didOpen", [&] {
					const std::string language_id = params.textDocument.languageId;
					compiler.openDocument(
						params.textDocument.uri,
						language_id,
						params.textDocument.version,
						params.textDocument.text
					);

					compiler.publishDiagnostics(params.textDocument.uri);
				});
			})
			.onTextDocumentDidChange([this, &compiler](lsp::DidChangeTextDocumentParams&& params) {
				orAbort(*endpoint, "textDocument/didChange", [&] {
					compiler.updateDocument(
						params.textDocument.uri, params.textDocument.version, params.contentChanges
					);
					compiler.publishDiagnostics(params.textDocument.uri);
				});
			})
			.onTextDocumentDidClose([this, &compiler](lsp::DidCloseTextDocumentParams&& params) {
				orAbort(*endpoint, "textDocument/didClose", [&] {
					compiler.closeDocument(params.textDocument.uri);
					compiler.publishDiagnostics(params.textDocument.uri);
				});
			})
			// @TODO: #3607 watched-file notifications are best-effort: clients are not required
		    // to support file watching, so creations and deletions on disk can go unnoticed.
			.onWorkspaceDidChangeWatchedFiles(
				[this, &compiler](lsp::DidChangeWatchedFilesParams&& params) {
					orAbort(*endpoint, "workspace/didChangeWatchedFiles", [&] {
						bool touched = false;
						for (const auto& change: params.changes) {
							if (change.type == lsp::FileChangeType::Changed) continue;
							compiler.fileCreatedOrDeletedOnDisk(change.uri);
							touched = true;
						}

						if (touched) compiler.publishDiagnostics({});
					});
				}
			)
			.onShutdown([]() -> lsp::ShutdownResult { return {}; })
			.onExit([]() {});
	}

	// NOLINTEND(cppcoreguidelines-rvalue-reference-param-not-moved)

}
