#include <lsp_interface/text_sync.hpp>

namespace duck_ls {

	namespace {
		/**
		 * @brief The number of UTF-16 code units the code point starting at `text[offset]` takes.
		 */
		u32 utf16UnitsAt(std::string_view text, usize offset) {
			const auto lead = static_cast<unsigned char>(text[offset]);
			if (lead >= 0xf0) return 2;
			return 1;
		}

		/**
		 * @brief The number of bytes the code point starting at `text[offset]` takes.
		 */
		usize codepointBytesAt(std::string_view text, usize offset) {
			const auto lead = static_cast<unsigned char>(text[offset]);
			if (lead >= 0xf0) return 4;
			if (lead >= 0xe0) return 3;
			if (lead >= 0xc0) return 2;
			return 1;
		}
	}

	void recomputeLineIndex(OpenDocument& document) {
		document.line_starts.clear();
		document.line_starts.push_back(0);
		document.is_ascii = true;

		for (usize i = 0; i < document.text.size(); ++i) {
			const auto c = static_cast<unsigned char>(document.text[i]);
			if (c >= 0x80) document.is_ascii = false;
			if (c == '\n') document.line_starts.push_back(static_cast<u32>(i + 1));
		}
	}

	base::Optional<u32> toByteOffset(const OpenDocument& document, const lsp::Position& position) {
		if (position.line >= document.line_starts.size()) return {};

		const usize line_start = document.line_starts[position.line];
		const usize line_end   = position.line + 1 < document.line_starts.size()
		                           ? document.line_starts[position.line + 1]
		                           : document.text.size();

		if (document.is_ascii) {
			const usize offset = line_start + position.character;
			if (offset > line_end) return {};
			return static_cast<u32>(offset);
		}

		usize offset    = line_start;
		u32   remaining = position.character;

		while (remaining > 0 && offset < line_end) {
			const auto units = utf16UnitsAt(document.text, offset);
			if (units > remaining) break;
			remaining -= units;
			offset += codepointBytesAt(document.text, offset);
		}

		if (remaining > 0) return {};
		return static_cast<u32>(offset);
	}

	bool applyChange(OpenDocument& document, const lsp::TextDocumentContentChangeEvent& change) {
		if (const auto* whole = std::get_if<lsp::TextDocumentContentChangeWholeDocument>(&change)) {
			document.text = whole->text;
			recomputeLineIndex(document);
			return true;
		}

		const auto* partial = std::get_if<lsp::TextDocumentContentChangePartial>(&change);
		if (partial == nullptr) return false;

		auto start = toByteOffset(document, partial->range.start);
		auto end   = toByteOffset(document, partial->range.end);
		if (start.empty() || end.empty() || start.value() > end.value()) return false;

		document.text.replace(start.value(), end.value() - start.value(), partial->text);
		recomputeLineIndex(document);
		return true;
	}

}
