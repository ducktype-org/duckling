#pragma once

#include <lsp/types.h>
#include <lsp_interface/open_documents.hpp>

#include <base/collections/optional.hpp>

namespace duck_ls {

	/**
	 * @brief Rebuilds `line_starts` and `is_ascii` from the document's current text.
	 */
	void recomputeLineIndex(OpenDocument& document);

	/**
	 * @brief Converts an LSP position into a byte offset into the document's text.
	 *
	 * The character component counts UTF-16 code units, as negotiated during initialize.
	 * Returns an empty optional when the position does not point into the document.
	 */
	base::Optional<u32> toByteOffset(const OpenDocument& document, const lsp::Position& position);

	/**
	 * @brief Applies one content change to the document, rebuilding the line index.
	 *
	 * Returns false when a partial change names a range the document does not contain, in which
	 * case the text is left untouched.
	 */
	bool applyChange(OpenDocument& document, const lsp::TextDocumentContentChangeEvent& change);

}
