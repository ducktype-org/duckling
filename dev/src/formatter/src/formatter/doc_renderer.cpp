#include "doc_renderer.hpp"

#include <base/types/ints.hpp>

#include <string_view>

namespace formatter {
	namespace {

		/**
		 * @brief Walks a Doc making every line-breaking decision.
		 *
		 * Tracks the current visual column incrementally: indentation contributes
		 * `indent levels * indent_width` columns (whether rendered as tabs or spaces),
		 * everything else one column per byte.
		 */
		class Renderer final {
		public:
			explicit Renderer(const FormatConfig& config): config(config) {}

			std::string run(const Doc& doc) {
				render(doc, Mode::Broken);
				return std::move(out);
			}

		private:
			enum class Mode {
				Flat,   ///< Everything on one line: Lines flatten, Groups and Fills stay inline.
				Broken  ///< Lines break; Groups and Fills decide their own layout.
			};

			const FormatConfig& config;
			std::string         out;
			u32                 column = 0;
			u32                 indent = 0;

			[[nodiscard]]
			bool fits(u32 width) const {
				return width != WIDTH_INFINITE && column + width <= config.max_line_length;
			}

			void write(std::string_view text) {
				out += text;
				column += static_cast<u32>(text.size());
			}

			void breakLine(u32 blanks = 0) {
				out.append(blanks + 1, '\n');
				if (config.indent_style == IndentStyle::Tab)
					out.append(indent, '\t');
				else
					out.append(static_cast<usize>(indent) * config.indent_width, ' ');
				column = indent * config.indent_width;
			}

			void renderAll(const std::vector<Doc>& children, Mode mode) {
				for (const auto& child: children) render(child, mode);
			}

			void render(const Doc& doc, Mode mode) {
				switch (doc.kind) {
				case DocKind::Text:
					write(doc.text);
					break;
				case DocKind::Concat:
					renderAll(doc.children, mode);
					break;
				case DocKind::Indent:
					indent++;
					renderAll(doc.children, mode);
					indent--;
					break;
				case DocKind::Line:
					if (mode == Mode::Flat) {
						if (doc.spaced) write(" ");
					} else {
						breakLine(doc.blanks);
					}
					break;
				case DocKind::Group:
					renderGroup(doc, mode);
					break;
				case DocKind::Fill:
					renderFill(doc, mode);
					break;
				case DocKind::LineComment:
					renderLineComment(doc.text);
					break;
				}
			}

			void renderGroup(const Doc& doc, Mode mode) {
				if (mode == Mode::Flat || (!doc.must_break && !doc.breakable)
				    || (!doc.must_break && fits(doc.flat_width))) {
					renderAll(doc.children, Mode::Flat);
				} else {
					renderAll(doc.children, Mode::Broken);
				}
			}

			/**
			 * Greedy filling. The first child always continues the current line. Each further
			 * child starts a new line only when its flat width does not fit at the live column
			 * (so a child following an exploded group continues on the closing-bracket line).
			 * The first break indents the continuation lines one level deeper, once for the
			 * whole fill.
			 */
			void renderFill(const Doc& doc, Mode mode) {
				if (mode == Mode::Flat) {
					bool first = true;
					for (const auto& child: doc.children) {
						if (!first && doc.spaced) write(" ");
						first = false;
						render(child, Mode::Flat);
					}
					return;
				}

				bool broke = false;
				bool first = true;
				for (const auto& child: doc.children) {
					if (first) {
						render(child, Mode::Broken);
						first = false;
						continue;
					}
					const u32 separator = doc.spaced ? 1 : 0;
					if (child.flat_width != WIDTH_INFINITE && fits(child.flat_width + separator)) {
						if (doc.spaced) write(" ");
						render(child, Mode::Broken);
					} else {
						if (!broke) {
							indent++;
							broke = true;
						}
						breakLine();
						render(child, Mode::Broken);
					}
				}
				if (broke) indent--;
			}

			/**
			 * Renders a line comment, re-flowing it onto multiple `#` lines when over-long.
			 *
			 * A comment that fits is reproduced verbatim. An over-long comment is split at word
			 * boundaries; every continuation line repeats the comment prefix (`#`, `##`, ...) at
			 * the current indentation. A single word longer than the line length is kept intact.
			 * This is the one transformation that changes the token stream: one comment token
			 * becomes several with the same combined text.
			 */
			void renderLineComment(std::string_view text) {
				if (column + text.size() <= config.max_line_length) {
					write(text);
					return;
				}

				usize prefix_end = 0;
				while (prefix_end < text.size() && text[prefix_end] == '#') prefix_end++;
				const std::string_view prefix = text.substr(0, prefix_end);

				write(prefix);
				bool  line_has_word = false;
				usize pos           = prefix_end;
				while (pos < text.size()) {
					while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) pos++;
					if (pos >= text.size()) break;
					usize end = pos;
					while (end < text.size() && text[end] != ' ' && text[end] != '\t') end++;
					const auto word = text.substr(pos, end - pos);
					pos             = end;

					if (line_has_word && column + 1 + word.size() > config.max_line_length) {
						breakLine();
						write(prefix);
					}
					write(" ");
					write(word);
					line_has_word = true;
				}
			}
		};
	}

	std::string renderDoc(const Doc& doc, const FormatConfig& config) {
		return Renderer(config).run(doc);
	}
}
