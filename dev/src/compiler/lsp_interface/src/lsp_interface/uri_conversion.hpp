#pragma once

#include <lsp/uri.h>
#include <lsp_interface/open_documents.hpp>

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

namespace duck_ls {

	/**
	 * @brief Converts a client URI into the physical path it names.
	 *
	 * Returns an empty optional for anything that is not a valid `file:` URI, so that schemes
	 * like `untitled:` never reach path handling that would panic on them.
	 */
	base::Optional<fs::FilePath> toPhysicalPath(const lsp::Uri& uri);

	/**
	 * @brief Converts a path the compiler reported into the URI to answer the client with.
	 *
	 * A virtual path is echoed back in the spelling the client originally sent, because a
	 * physical path is canonicalized on the way into the module tree. Any other path is
	 * encoded from scratch.
	 */
	lsp::Uri toDocumentUri(const fs::FilePath& path, const OpenDocuments& documents);

}
