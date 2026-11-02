// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <lsp/types.h>
#include <lsp/uri.h>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ok_bad.hpp>

#include <filesystem/file_path.hpp>
#include <filesystem/vfs.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace duck_ls {

	/**
	 * @brief A document the editor currently holds open.
	 *
	 * The content is not here: it lives in the file at `cache_path`, which is what the compiler
	 * reads, so there is only ever one copy of it.
	 */
	struct OpenDocument final {
		/// The URI exactly as the client spelled it, used for every reply about this document.
		lsp::Uri uri;
		/// The document's place in the cache, its identity for the whole server.
		fs::FilePath cache_path;
		/// The client's language id, currently unused.
		std::string language_id{};
		/// The version the client last sent.
		i32 version = 0;
		/// Whether a change could not be applied, leaving the buffer behind the client's.
		bool out_of_sync = false;
		/// Whether a file backed the document when it was opened, deciding what didClose restores.
		/// Currently unused.
		bool existed_on_disk = false;
	};

	/**
	 * @brief The naming and buffering layer: converts between what the client says (URIs) and
	 * what the compiler reads (files), and holds the buffer of every open document.
	 *
	 * Two spellings of the same path exist. The *source* path is where the file really lives,
	 * either on disk or, in tests, in a virtual filesystem. The *cache* path is its twin in an
	 * overlay owned by this cache, and is the identity of a document for the whole server.
	 *
	 * Where source files live is the one thing that varies. With no source VFS the server reads
	 * the real filesystem; a test passes one instead, writes a file tree into it, and the whole
	 * server then resolves URIs into that tree.
	 */
	class FilesCache final {
	public:
		explicit FilesCache(base::Optional<base::Ref<fs::VFS>> source_vfs = {});

		FilesCache(const FilesCache&)            = delete;
		FilesCache& operator=(const FilesCache&) = delete;

		/**
		 * @brief Resolves a client URI to the file it names, empty when the URI is not one the
		 * server can name a file with.
		 */
		base::Optional<fs::FilePath> sourcePathFor(const lsp::Uri& uri);

		/**
		 * @brief Resolves a client URI to the cache path identifying that document.
		 */
		base::Optional<fs::FilePath> cachePathFor(const lsp::Uri& uri);

		/**
		 * @brief Maps a source file to its twin in the cache overlay.
		 */
		fs::FilePath cachePath(const fs::FilePath& source_path);

		/**
		 * @brief Maps a cache path back to the file behind it.
		 */
		fs::FilePath sourcePath(const fs::FilePath& cache_path);

		/**
		 * @brief The URI to name `path` with when answering the client.
		 *
		 * An open document is echoed back in the spelling the client originally sent; anything
		 * else is named by its own path.
		 */
		lsp::Uri uriFor(const fs::FilePath& path);

		/**
		 * @brief Whether a buffer stands in front of `source_file`.
		 *
		 * Answered from the overlay, so it holds for any file the walk of a package runs into,
		 * not only for one the client named.
		 */
		bool isOpened(const fs::FilePath& source_file);

		/**
		 * @brief Whether the client currently holds the document at `uri` open.
		 *
		 * Answered from the open records, so it is false for a URI this cache cannot name a
		 * file with.
		 */
		bool isOpened(const lsp::Uri& uri);

		/**
		 * @brief Registers a workspace root, bounding the upward search for a package root.
		 */
		void addWorkspaceRoot(const fs::FilePath& root);

		/**
		 * @brief Finds the workspace root containing `path`, comparing whole path components.
		 */
		base::Optional<fs::FilePath> workspaceRootFor(const fs::FilePath& path);

		/**
		 * @brief Creates the buffer for a newly opened document and records it as open.
		 *
		 * @return The cache path the buffer was created at, or empty when the URI is unsupported.
		 */
		base::Optional<fs::FilePath> openDocument(
			const lsp::Uri& uri, std::string_view language_id, i32 version, std::string_view text
		);

		/**
		 * @brief Applies the client's changes to the buffer of an already open document,
		 * replacing each range they name with the text they carry.
		 *
		 * @return Bad when the URI names no file, when the document is not open, when the
		 * version went backwards, or when a change named a range the buffer does not contain,
		 * in which case the buffer is left untouched.
		 */
		base::OkBad updateDocument(
			const lsp::Uri&                                        uri,
			i32                                                    version,
			const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
		);

		/**
		 * @brief Drops the buffer and the open record of a document.
		 *
		 * @return Bad when the URI names no file, or when nothing was open there.
		 */
		base::OkBad closeDocument(const lsp::Uri& uri);

		/**
		 * @brief Looks up the document open at `cache_path`.
		 */
		base::Optional<base::Ref<OpenDocument>> find(const fs::FilePath& cache_path);

	private:
		/// Test only, if set, the FilesCache teats this fs::VFS like the physical one.
		base::Optional<base::Ref<fs::VFS>>             source_vfs{};
		fs::VFS                                        cache_vfs{};
		std::unordered_map<fs::FilePath, OpenDocument> documents{};
		std::vector<fs::FilePath>                      workspace_roots{};
	};

}
