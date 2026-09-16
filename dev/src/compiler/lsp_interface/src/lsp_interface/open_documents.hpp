#pragma once

#include <lsp/uri.h>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file_path.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace duck_ls {

	/**
	 * @brief A document the editor currently holds open, together with everything needed to
	 * splice incremental changes into it and to answer the client in its own URI spelling.
	 */
	struct OpenDocument final {
		/// The URI exactly as the client spelled it, used for every reply about this document.
		lsp::Uri uri;
		/// The physical path identifying the document, independent of the client's spelling.
		fs::FilePath physical_path;
		/// The client's language id, deciding between a program and a script parse.
		std::string language_id;
		/// The version the client last sent.
		i32 version = 0;
		/// Whether the file existed on disk when it was opened, deciding what didClose restores.
		bool existed_on_disk = false;
		/// The current buffer content.
		std::string text;
		/// Byte offset of the first character of every line, rebuilt whenever the text changes.
		std::vector<u32> line_starts;
		/// True while the buffer is pure ASCII, making a UTF-16 column a plain byte offset.
		bool is_ascii = true;
	};

	/**
	 * @brief The set of documents open in the editor, and the authority on whether a path is open.
	 *
	 * Documents are keyed by physical path rather than by the client's URI: the path is what the
	 * compiler reports back, and it is the URI normalized, so two spellings of the same file
	 * resolve to one document.
	 */
	class OpenDocuments final {
	public:
		/**
		 * @brief Inserts or replaces the document for `document.physical_path`.
		 */
		base::Ref<OpenDocument> insert(OpenDocument document);

		/**
		 * @brief Drops the document for `path`, returning false when nothing was open there.
		 */
		bool erase(const fs::FilePath& path);

		/**
		 * @brief Looks up the document open at `path`.
		 */
		base::Optional<base::Ref<OpenDocument>> find(const fs::FilePath& path);

		/**
		 * @brief Const overload of the path lookup, used by URI conversion.
		 */
		[[nodiscard]] base::Optional<base::CRef<OpenDocument>> find(const fs::FilePath& path) const;

		/**
		 * @brief Whether any document is open at `path`.
		 */
		[[nodiscard]] bool contains(const fs::FilePath& path) const;

		[[nodiscard]] bool empty() const { return documents.empty(); }

	private:
		std::unordered_map<fs::FilePath, OpenDocument> documents;
	};

}
