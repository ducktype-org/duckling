#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>

#include <base/pointers/ref.hpp>
#include <base/types/ok_bad.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <string_view>
#include <unordered_map>
#include <vector>

namespace compiler::frontend {
	class ModuleTree;
}

namespace duck_ls {

	/**
	 * @brief Derives a package ID that stays the same across reloads of the same directory.
	 *
	 * The package ID is the root of every component hash, so a random one would make every
	 * reload a full recompile.
	 */
	base::StrID packageIdForRoot(const fs::FilePath& package_root);

	/**
	 * @brief Everything the protocol handlers ask of the compiler: it keeps the module tree in
	 * step with what the editor holds open, and answers with the diagnostics of the resulting
	 * compilation.
	 */
	class Compiler {
	public:
		Compiler(base::Ref<ServerSession> session, base::Ref<FilesCache> files);

		Compiler(const Compiler&)            = delete;
		Compiler& operator=(const Compiler&) = delete;
		virtual ~Compiler()                  = default;

		/**
		 * @brief Registers a workspace root, bounding the upward search for a package root.
		 */
		virtual void addWorkspace(const lsp::Uri& root);

		/**
		 * @brief Makes the document resolve to the editor's buffer instead of its own content.
		 */
		virtual void openDocument(
			const lsp::Uri& uri, std::string_view language_id, i32 version, std::string_view text
		);

		/**
		 * @brief Applies the client's changes to the buffer and reparses what depended on it.
		 *
		 * @return Bad when the document was not open, so nothing was recompiled.
		 */
		virtual base::OkBad updateDocument(
			const lsp::Uri&                                        uri,
			i32                                                    version,
			const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
		);

		/**
		 * @brief Makes the document resolve to the file behind it again.
		 *
		 * @return Bad when the document was not open, so nothing was swapped back.
		 */
		virtual base::OkBad closeDocument(const lsp::Uri& uri);

		/**
		 * @brief Reacts to a file appearing or disappearing outside the editor.
		 */
		virtual void fileCreatedOrDeletedOnDisk(const lsp::Uri& uri);

		/**
		 * @brief Compiles the package the document belongs to and pushes its diagnostics.
		 *
		 * The URI only says which package to compile: the diagnostics come from all of it, not
		 * just from that one document. Files that had diagnostics before and have none now are
		 * pushed with an empty array so that the client clears them.
		 */
		virtual void publishDiagnostics(const lsp::Uri& uri);

	private:
		/**
		 * @brief Finds the package root owning `path`, bounded by the workspace root.
		 */
		base::Optional<fs::FilePath> walkToPackageRoot(const fs::FilePath& path);

		/**
		 * @brief Walks `package_root` into the module tree and registers it as a package.
		 */
		void loadPackage(const fs::FilePath& package_root);

		/**
		 * @brief Tears the package owning `path` down and walks it again from its source.
		 */
		void reloadPackageOwning(const fs::FilePath& path);

		/**
		 * @brief Swaps the file backing `module` from `current` to `replacement`, invalidating
		 * only the queries that depended on the old one.
		 *
		 * @pre `module` is the module whose main source file is `current`, which is what
		 * `findModuleForFile(current)` answers; the caller resolves it and decides what to do
		 * when the file is not registered at all.
		 */
		void swapMainSourceFile(
			base::Ref<compiler::frontend::ModuleTree> module,
			const fs::File&                           current,
			const fs::File&                           replacement
		);

		base::Ref<ServerSession> session;
		base::Ref<FilesCache>    files;
		/// The last diagnostics pushed per URI, so that stale ones can be cleared.
		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> last_published_diagnostics;
	};
}
