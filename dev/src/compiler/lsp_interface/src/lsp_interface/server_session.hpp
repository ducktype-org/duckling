#pragma once

#include <lsp/messages.h>
#include <lsp/types.h>
#include <lsp_interface/files_management.hpp>
#include <lsp_interface/open_documents.hpp>

#include <base/pointers/box.hpp>

#include <filesystem/file_path.hpp>
#include <filesystem/vfs.hpp>

#include <unordered_map>
#include <vector>

namespace duck_ls {

	/**
	 * @brief All state one language server instance owns, created in main and handed to the
	 * handlers by reference so that a test can drive the same server in-process.
	 */
	struct ServerSession final {
		ServerSession() = default;

		/// Overlay holding the content of open buffers.
		fs::VFS cache_vfs;
		/// The documents the editor currently holds open.
		OpenDocuments documents;
		/// The roots the client reported, bounding the upward search for a package root.
		std::vector<fs::FilePath> workspace_roots;
		/// The compiler side of the server, or a recording double in tests. Set after construction
		/// because the compiler implementation needs a reference back to this session.
		base::Optional<base::Box<IFilesManagement>> files;
		/// The encoding positions are expressed in, as negotiated during initialize.
		lsp::PositionEncodingKind position_encoding = lsp::PositionEncodingKind::UTF16;
		/// The last diagnostics published per URI, so that stale ones can be cleared.
		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> last_published_diagnostics;

		/**
		 * @brief Maps a source path to its twin in the session cache.
		 *
		 * LSP code must never build a cache path from a literal: a path spelled as a string
		 * binds to the singleton VFS instead of this session's overlay.
		 */
		/**
		 * @brief Installs the compiler side of the server.
		 */
		void setFiles(base::Box<IFilesManagement> files_management);

		/**
		 * @brief The compiler side of the server; panics when it has not been installed.
		 */
		IFilesManagement& filesManagement();

		fs::FilePath cachePath(const fs::FilePath& source);

		/**
		 * @brief Registers a workspace root, ignoring one that is already known.
		 */
		void addWorkspaceRoot(const fs::FilePath& root);

		/**
		 * @brief Finds the workspace root containing `path`, comparing whole path components.
		 */
		[[nodiscard]] base::Optional<fs::FilePath> workspaceRootFor(const fs::FilePath& path) const;
	};

}
