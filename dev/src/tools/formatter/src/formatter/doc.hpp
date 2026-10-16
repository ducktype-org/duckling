/**
 * @file doc.hpp
 * @brief The formatter's layout document (Doc IR).
 *
 * A Doc is a tree describing formatted output with the line-breaking decisions left open.
 * The builder (doc_builder) translates tokens into a Doc; the renderer (doc_renderer) then
 * makes every breaking decision against the configured line length. Each node caches the
 * width of its single-line ("flat") rendering, so a fits-on-this-line check is O(1).
 */
#pragma once

#include <base/types/ints.hpp>

#include <string>
#include <vector>

namespace formatter {

	enum class DocKind {
		/**
		 * A literal piece of output. Normally a single line; a multi-line `#{ ... #}` block
		 * comment is the one case that carries newlines, and it then forces enclosing groups
		 * to break (`must_break`).
		 */
		Text,
		/** Children rendered in order. */
		Concat,
		/** Children rendered one indentation level deeper. */
		Indent,
		/**
		 * A potential line break. Flat: a single space (when `spaced`) or nothing.
		 * Broken: `blanks` empty lines, then a newline and the current indentation.
		 */
		Line,
		/**
		 * A bracket group deciding its own layout: flat when it fits (or is not `breakable`),
		 * otherwise broken so that every Line inside it (elements) starts a new line.
		 * Groups nested in a flat context stay flat.
		 */
		Group,
		/**
		 * Greedy filling: the first child always continues the current line; each further
		 * child starts a new line (indented one level deeper) only when it does not fit.
		 * `spaced` children are separated by a single space when they share a line.
		 */
		Fill,
		/**
		 * A `# ...` line comment. Rendered verbatim when it fits; otherwise re-flowed at word
		 * boundaries onto several lines repeating the comment prefix. Forces every enclosing
		 * Group to break (a line comment consumes the rest of its line).
		 */
		LineComment,
	};

	/** Flat width of content that can never render on one line. */
	inline constexpr u32 WIDTH_INFINITE = 0xFF'FF'FF'FF;

	struct Doc final {
		DocKind          kind = DocKind::Text;
		std::string      text;      ///< Text, LineComment: the literal content.
		std::vector<Doc> children;  ///< Concat, Indent, Group, Fill.

		u32  blanks    = 0;         ///< Line: empty lines to keep when broken.
		bool spaced    = false;     ///< Line: flat form is a space. Fill: space-separated children.
		bool breakable = false;     ///< Group: may break when over-long.

		u32  flat_width = 0;        ///< Cached width of the flat rendering (saturating).
		bool must_break = false;    ///< Contains a line comment, so flat rendering is impossible.
	};

	namespace doc {

		[[nodiscard]]
		Doc text(std::string content);

		[[nodiscard]]
		Doc space();

		[[nodiscard]]
		Doc concat(std::vector<Doc> children);

		[[nodiscard]]
		Doc indent(std::vector<Doc> children);

		/** A break that flattens to a single space, keeping @p blanks empty lines when broken. */
		[[nodiscard]]
		Doc line(u32 blanks = 0);

		/** A break that flattens to nothing. */
		[[nodiscard]]
		Doc softLine();

		[[nodiscard]]
		Doc group(bool breakable, std::vector<Doc> children);

		[[nodiscard]]
		Doc fill(bool spaced, std::vector<Doc> children);

		[[nodiscard]]
		Doc lineComment(std::string content);
	}
}
