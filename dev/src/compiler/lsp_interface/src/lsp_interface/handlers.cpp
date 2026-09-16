#include <lsp/error.h>
#include <lsp/protocol_version.h>
#include <lsp_interface/diagnostics.hpp>
#include <lsp_interface/handlers.hpp>
#include <lsp_interface/text_sync.hpp>
#include <lsp_interface/uri_conversion.hpp>

#include <iostream>

namespace duck_ls {

	namespace {
#if LSP_PROTOCOL_VERSION < LSP_INT_VERSION(3, 18, 0)
		using ServerInfo = lsp::InitializeResultServerInfo;
#else
		using ServerInfo = lsp::ServerInfo;
#endif

		/**
		 * @brief Resolves a client URI, rejecting anything that does not name a file on disk.
		 */
		fs::FilePath requireFilePath(const lsp::Uri& uri) {
			auto path = toPhysicalPath(uri);
			if (path.empty())
				throw lsp::RequestError(
					lsp::MessageError::InvalidParams, "Unsupported document URI: " + uri.toString()
				);
			return path.value();
		}

		/**
		 * @brief Collects the workspace roots the client reported, old and new style alike.
		 */
		void collectWorkspaceRoots(const lsp::InitializeParams& params, ServerSession& session) {
			if (params.workspaceFolders.has_value() && !params.workspaceFolders->isNull())
				for (const auto& folder: params.workspaceFolders->value())
					if_opt_some(toPhysicalPath(folder.uri), path) session.addWorkspaceRoot(path);

			if (!params.rootUri.isNull())
				if_opt_some(toPhysicalPath(params.rootUri.value()), path)
					session.addWorkspaceRoot(path);

			for (const auto& root: session.workspace_roots)
				session.filesManagement().addWorkspace(root);
		}

		/**
		 * @brief Recomputes and publishes diagnostics, keeping a compiler failure from tearing
		 * down the connection.
		 */
		void publish(
			lsp::ServerEndpoint& endpoint, ServerSession& session, const fs::FilePath& path
		) {
			try {
				publishDiagnostics(endpoint, session, path);
			} catch (const std::exception& e) {
				std::cerr << "duck_ls: diagnostics failed for " << path.strView() << ": "
						  << e.what() << "\n";
			}
		}

		lsp::InitializeResult initialize(
			const lsp::InitializeParams& params, ServerSession& session
		) {
			collectWorkspaceRoots(params, session);

			return {
				.capabilities = {
					.positionEncoding = session.position_encoding,
					.textDocumentSync = lsp::TextDocumentSyncOptions{
						.openClose = true,
						.change    = lsp::TextDocumentSyncKind::Incremental,
					},
				},
				.serverInfo = ServerInfo{
					.name    = "duck_ls",
					.version = "0.1.0",
				},
			};
		}
	}

	void registerHandlers(lsp::ServerEndpoint& endpoint, ServerSession& session) {
		endpoint
			.onInitialize(
				[&session](
					const lsp::InitializeParams& params
				) -> lsp::RequestResult<lsp::InitializeResult> {
					return initialize(params, session);
				}
			)
			.onInitialized([](auto&&) {})
			.onTextDocumentDidOpen([&endpoint, &session](lsp::DidOpenTextDocumentParams&& params) {
				auto path = requireFilePath(params.textDocument.uri);

				OpenDocument document{
					.uri             = params.textDocument.uri,
					.physical_path   = path,
					.language_id     = params.textDocument.languageId,
					.version         = params.textDocument.version,
					.existed_on_disk = path.isRegularFile(),
					.text            = std::move(params.textDocument.text),
					.line_starts     = {},
					.is_ascii        = true,
				};

				auto opened = session.documents.insert(std::move(document));
				recomputeLineIndex(*opened);
				session.filesManagement().openDocument(path, opened->text);
				publish(endpoint, session, path);
			})
			.onTextDocumentDidChange([&endpoint,
		                              &session](lsp::DidChangeTextDocumentParams&& params) {
				auto path     = requireFilePath(params.textDocument.uri);
				auto document = session.documents.find(path);

				if (document.empty()) {
					std::cerr << "duck_ls: change for a document that is not open: "
							  << path.strView() << "\n";
					return;
				}

				// Versions only ever increase; anything else means the buffers have diverged
			    // and splicing at the client's offsets would silently corrupt the text.
				if (params.textDocument.version <= document.value()->version) {
					std::cerr << "duck_ls: out of order version for " << path.strView() << ": got "
							  << params.textDocument.version << " after "
							  << document.value()->version << ", ignoring\n";
					return;
				}

				for (const auto& change: params.contentChanges) {
					if (applyChange(*document.value(), change)) continue;

					std::cerr << "duck_ls: change out of range for " << path.strView()
							  << ", ignoring the rest of the notification\n";
					return;
				}

				document.value()->version = params.textDocument.version;
				session.filesManagement().updateDocument(path, document.value()->text);
				publish(endpoint, session, path);
			})
			.onTextDocumentDidClose([&endpoint, &session](lsp::DidCloseTextDocumentParams&& params) {
				auto path = requireFilePath(params.textDocument.uri);
				if (!session.documents.erase(path)) return;
				session.filesManagement().closeDocument(path);
				publish(endpoint, session, path);
			})
			.onShutdown([]() -> lsp::ShutdownResult { return {}; })
			.onExit([]() {});
	}

}
