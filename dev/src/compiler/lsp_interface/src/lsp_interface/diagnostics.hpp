#pragma once

#include <lsp/server_endpoint.h>
#include <lsp_interface/server_session.hpp>

#include <filesystem/file.hpp>

#include <unordered_map>
#include <vector>

namespace duck_ls {

	/**
	 * @brief Compiles the package `file` belongs to and returns its diagnostics, grouped by the
	 * URI of the file each one belongs to.
	 *
	 * Files are named the way the client named them, so `documents` supplies the spelling of
	 * everything the editor holds open.
	 *
	 * @warning It compiles the entire package the file belongs to, so also modules unrelated to
	 * the file.
	 */
	std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> collectDiagnostics(
		const fs::File& file, const OpenDocuments& documents
	);

	/**
	 * @brief Compiles the package `path` belongs to and publishes its diagnostics.
	 *
	 * Files that had diagnostics before and have none now are published with an empty array, so
	 * the client clears them.
	 */
	void publishDiagnostics(
		lsp::ServerEndpoint& endpoint, ServerSession& session, const fs::FilePath& path
	);

}
