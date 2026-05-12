#include "replxx.hpp"

#include <repl/helpers.hpp>

#include <base/types/ints.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace compiler::repl {
	static constexpr std::string_view TAB_SPACES = "    ";

	// ─── Duckling language keywords ──────────────────────────────────────────────

	// clang-format off
	static const std::set<std::string> DUCKLING_KEYWORDS = {
		"alias", "and", "as", "assert", "block", "box", "break", "case", "catch",
		"class", "compile_assert", "const", "continue", "copy", "debug", "defer",
		"dict", "else", "expand", "extends", "extern", "false", "for", "fun",
		"fundecl", "if", "implements", "import", "in", "lambda", "let", "loop",
		"match", "move", "namespace", "none", "not", "or", "pattern", "private",
		"protected", "public", "redo", "ref", "slice", "ptr", "manyptr", "cptr", 
		"refof", "restart", "return", "set", "sizeof", "static", "str", "switch",
		"test", "then", "this", "throw", "true", "try", "type", "using", "var",
		"vec", "while", "with", "xor"
	};

	static const std::set<std::string> DUCKLING_TYPES = {
		"i8", "i16", "i32", "i64", "i128",
		"u8", "u16", "u32", "u64", "u128",
		"f16", "f32", "f64", "f80", "f128",
		"char", "bool", "str", "type",
		"vec", "set", "dict", "array"
	};
	// clang-format on

	// ─── Helpers ─────────────────────────────────────────────────────────────────

	/// Extract a word (identifier-like) ending at position \p pos in \p input.
	static std::string extractWordEndingAt(const std::string& input, usize pos) {
		if (pos == 0 || pos > input.length()) return "";

		usize start = pos;
		while (start > 0
		       && (std::isalnum(static_cast<unsigned char>(input[start - 1]))
		           || input[start - 1] == '_')) {
			--start;
		}

		return input.substr(start, pos - start);
	}

	/// Extract all identifier-like tokens from \p text.
	static std::vector<std::string> tokenizeIdentifiers(const std::string& text) {
		std::vector<std::string> tokens;
		std::string              current;
		for (char ch: text) {
			if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
				current += ch;
			} else if (!current.empty()) {
				tokens.push_back(std::move(current));
				current.clear();
			}
		}
		if (!current.empty()) tokens.push_back(std::move(current));
		return tokens;
	}

	/**
	 * Compute indentation depth from unmatched braces up to \p cursor_pos.
	 * Braces inside strings or line comments are ignored.
	 */
	static int computeBraceIndentDepth(const std::string& input, usize cursor_pos) {
		int  depth           = 0;
		bool in_single_quote = false;
		bool in_double_quote = false;
		bool escape_next     = false;

		bool in_single_line_comment  = false;
		int  multiline_comment_depth = 0;

		for (usize i = 0; i < cursor_pos && i < input.size(); ++i) {
			const char ch = input[i];

			if (escape_next) {
				escape_next = false;
				continue;
			}

			if (in_single_line_comment) {
				if (ch == '\n') in_single_line_comment = false;
				continue;
			}

			if (in_single_quote) {
				if (ch == '\\')
					escape_next = true;
				else if (ch == '\'')
					in_single_quote = false;
				continue;
			}

			if (in_double_quote) {
				if (ch == '\\')
					escape_next = true;
				else if (ch == '"')
					in_double_quote = false;
				continue;
			}

			bool has_next = (i + 1 < cursor_pos && i + 1 < input.size());
			char next_ch  = has_next ? input[i + 1] : '\0';

			if (ch == '#' && next_ch == '{') {
				multiline_comment_depth++;
				++i;  // Skip the '{'
				continue;
			}

			if (ch == '#' && next_ch == '}') {
				if (multiline_comment_depth > 0) {
					multiline_comment_depth--;
					++i;  // Skip the '}'
					continue;
				}
			}

			if (ch == '#') {
				in_single_line_comment = true;
				continue;
			}

			if (multiline_comment_depth > 0) continue;

			if (ch == '\'') {
				in_single_quote = true;
				continue;
			}

			if (ch == '"') {
				in_double_quote = true;
				continue;
			}

			if (ch == '{')
				++depth;
			else if (ch == '}')
				depth = std::max(0, depth - 1);
		}

		return depth;
	}

	/**
	 * Constructs a simplified string where every UTF-8 code point from the input
	 * is represented by exactly 1 byte.
	 * Single-byte characters are kept as-is, while multi-byte characters
	 * are replaced by a specified constant byte.
	 *
	 * \param input - a UTF-8 encoded string.
	 * \param constant_byte - the character to substitute for multi-byte code points.
	 * By default it is '\1' - doesn't fall into any category and will be default color.
	 * \return A string whose .length() perfectly matches the number of code points.
	 */
	std::string mapUtf8CodePoints(const std::string& input, char constant_byte = '\1') {
		std::string result;

		result.reserve(input.length());

		for (size_t i = 0; i < input.length(); ++i) {
			unsigned char c = static_cast<unsigned char>(input[i]);

			if ((c & 0x80) == 0x00) {
				// 1-byte code point (ASCII: 0xxxxxxx)
				result.push_back(input[i]);
			} else if ((c & 0xC0) == 0xC0) {
				// Leading byte of a multi-byte code point (11xxxxxx)
				// We substitute it with our chosen constant byte.
				result.push_back(constant_byte);
			} else {
				// Continuation byte (10xxxxxx)
				// We simply ignore these, so the multi-byte code point
				// only contributes exactly 1 byte to the `result` string.
			}
		}

		return result;
	}

	// ─── History file path ───────────────────────────────────────────────────────

	std::string FrontendReplxxImplementation::getHistoryFilePath() {
		const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe)
		if (home != nullptr) return std::string(home) + "/.duckling_repl_history";
		return ".duckling_repl_history";
	}

	// ─── Construction / destruction ──────────────────────────────────────────────

	FrontendReplxxImplementation::FrontendReplxxImplementation(bool completions_enabled):
		  m_completions_enabled(completions_enabled) {
		m_replxx.set_max_history_size(1'000);
		m_replxx.set_word_break_characters(" \t\n;,+-/*%^&|~<>=!?@#$:(){}[]");
		m_replxx.set_indent_multiline(true);
		m_replxx.set_unique_history(true);
		m_replxx.set_double_tab_completion(false);
		m_replxx.set_complete_on_empty(false);
		m_replxx.set_beep_on_ambiguous_completion(false);
		m_replxx.set_max_hint_rows(8);
		m_replxx.set_hint_delay(0);
		m_replxx.enable_bracketed_paste();

		// Load persistent history from file.
		m_replxx.history_load(getHistoryFilePath());

		setupKeyBindings();
		setupHighlighter();
		if (m_completions_enabled) {
			setupCompletion();
			setupHints();
		}
	}

	FrontendReplxxImplementation::~FrontendReplxxImplementation() {
		// Persist history to disk.
		m_replxx.history_save(getHistoryFilePath());
	}

	// ─── Key bindings ────────────────────────────────────────────────────────────

	void FrontendReplxxImplementation::setupKeyBindings() {
		using Replxx = replxx::Replxx;

		// ── Enter → commit
		m_replxx.bind_key_internal(Replxx::KEY::ENTER, "commit_line");
		m_replxx.bind_key(Replxx::KEY::TAB, [this](char32_t code) {
			auto        state      = m_replxx.get_state();
			std::string line       = (state.text() != nullptr) ? state.text() : "";
			int         cursor_pos = state.cursor_position();

			if (cursor_pos < 0) cursor_pos = static_cast<int>(line.size());
			if (cursor_pos > static_cast<int>(line.size()))
				cursor_pos = static_cast<int>(line.size());

			if (m_completions_enabled) {
				std::string input_to_cursor = line.substr(0, static_cast<usize>(cursor_pos));
				std::string prefix = extractWordEndingAt(input_to_cursor, input_to_cursor.size());

				bool has_completions = false;
				if (!prefix.empty()) {
					auto can_complete = [&prefix](const std::string& candidate) {
						return candidate.size() > prefix.size() && candidate.starts_with(prefix);
					};

					has_completions = std::ranges::any_of(DUCKLING_KEYWORDS, can_complete)
					               || std::ranges::any_of(DUCKLING_TYPES, can_complete)
					               || std::ranges::any_of(m_user_words, can_complete);
				}

				if (has_completions) return m_replxx.invoke(Replxx::ACTION::COMPLETE_LINE, code);
			}

			line.insert(static_cast<usize>(cursor_pos), TAB_SPACES);
			m_replxx.set_state(
				Replxx::State(line.c_str(), cursor_pos + static_cast<int>(TAB_SPACES.size()))
			);
			return Replxx::ACTION_RESULT::CONTINUE;
		});

		auto insert_newline_with_auto_indent = [this](char32_t /*code*/) {
			auto        state      = m_replxx.get_state();
			std::string line       = (state.text() != nullptr) ? state.text() : "";
			int         cursor_pos = state.cursor_position();

			if (cursor_pos < 0) cursor_pos = static_cast<int>(line.size());
			if (cursor_pos > static_cast<int>(line.size()))
				cursor_pos = static_cast<int>(line.size());

			const int indent_depth = computeBraceIndentDepth(line, static_cast<usize>(cursor_pos));

			std::string indentation;
			indentation.reserve(static_cast<usize>(indent_depth) * TAB_SPACES.size());
			for (int i = 0; i < indent_depth; ++i) indentation += TAB_SPACES;

			const std::string insertion = "\n" + indentation;
			line.insert(static_cast<usize>(cursor_pos), insertion);

			m_replxx.set_state(
				Replxx::State(line.c_str(), cursor_pos + static_cast<int>(insertion.size()))
			);

			return Replxx::ACTION_RESULT::CONTINUE;
		};

		// ── Alt+Enter → newline ──
		m_replxx.bind_key(Replxx::KEY::BASE_META | '\r', insert_newline_with_auto_indent);
		m_replxx.bind_key(Replxx::KEY::meta(Replxx::KEY::ENTER), insert_newline_with_auto_indent);

		// ── F2 → new line (fallback for weird terminals) ──
		m_replxx.bind_key(Replxx::KEY::F2, insert_newline_with_auto_indent);
	}

	// ─── Syntax highlighting ─────────────────────────────────────────────────────

	void FrontendReplxxImplementation::setupHighlighter() {
		using Color = replxx::Replxx::Color;

		m_replxx.set_highlighter_callback([](const std::string&        input,
		                                     replxx::Replxx::colors_t& colors) {
			// We iterate over the input, identifying tokens and coloring them.
			usize i                 = 0;
			auto   input_code_points = mapUtf8CodePoints(input);
			usize len               = input_code_points.size();

			while (i < len) {
				// Skip whitespace
				if (std::isspace(static_cast<unsigned char>(input_code_points[i]))) {
					++i;
					continue;
				}

				// String literal (double-quoted)
				if (input_code_points[i] == '"') {
					usize start = i;
					++i;
					while (i < len && input_code_points[i] != '"') {
						if (input_code_points[i] == '\\' && i + 1 < len) ++i;  // skip escape
						++i;
					}
					if (i < len) ++i;  // closing quote
					for (usize j = start; j < i && j < colors.size(); ++j)
						colors[j] = Color::BRIGHTGREEN;
					continue;
				}

				// String literal (single-quoted / char literal)
				if (input_code_points[i] == '\'') {
					usize start = i;
					++i;
					while (i < len && input_code_points[i] != '\'') {
						if (input_code_points[i] == '\\' && i + 1 < len) ++i;
						++i;
					}
					if (i < len) ++i;
					for (usize j = start; j < i && j < colors.size(); ++j)
						colors[j] = Color::BRIGHTGREEN;
					continue;
				}

				// Line comment (//)
				if (input_code_points[i] == '/' && i + 1 < len && input_code_points[i + 1] == '/') {
					for (usize j = i; j < colors.size(); ++j) colors[j] = Color::GRAY;
					break;  // rest of line is comment
				}

				// Numeric literal
				if (std::isdigit(static_cast<unsigned char>(input_code_points[i]))
				    || (input_code_points[i] == '.' && i + 1 < len
				        && std::isdigit(static_cast<unsigned char>(input_code_points[i + 1])))) {
					usize start = i;
					while (i < len
					       && (std::isalnum(static_cast<unsigned char>(input_code_points[i]))
					           || input_code_points[i] == '.' || input_code_points[i] == '_'))
						++i;
					for (usize j = start; j < i && j < colors.size(); ++j)
						colors[j] = Color::YELLOW;
					continue;
				}

				// Identifier or keyword
				if (std::isalpha(static_cast<unsigned char>(input_code_points[i]))
				    || input_code_points[i] == '_') {
					usize start = i;
					while (i < len
					       && (std::isalnum(static_cast<unsigned char>(input_code_points[i]))
					           || input_code_points[i] == '_'))
						++i;
					std::string word = input_code_points.substr(start, i - start);

					Color color = Color::DEFAULT;
					if (DUCKLING_KEYWORDS.count(word) != 0)
						color = Color::BRIGHTCYAN;
					else if (DUCKLING_TYPES.count(word) != 0)
						color = Color::BRIGHTMAGENTA;
					else if (word == "true" || word == "false" || word == "none")
						color = Color::YELLOW;

					for (usize j = start; j < i && j < colors.size(); ++j) colors[j] = color;
					continue;
				}

				// REPL command (starts with /)
				if (input_code_points[i] == '/' && i == 0) {
					for (auto& color: colors) color = Color::BRIGHTBLUE;
					return;
				}

				// Operator characters
				if (std::string_view("+-*/%^&|~<>=!?@#$:.->.").find(input_code_points[i])
				    != std::string_view::npos) {
					if (i < colors.size()) colors[i] = Color::BROWN;
					++i;
					continue;
				}

				++i;  // anything else — default color
			}
		});
	}

	// ─── Tab completion ──────────────────────────────────────────────────────────

	void FrontendReplxxImplementation::setupCompletion() {
		m_replxx.set_completion_callback(
			[this](const std::string& input, int& context_len) -> replxx::Replxx::completions_t {
				using Color = replxx::Replxx::Color;

				replxx::Replxx::completions_t completions;
				std::string                   prefix = extractWordEndingAt(input, input.size());
				if (prefix.empty()) return completions;

				context_len = static_cast<int>(prefix.size());

				// Collect candidates from keywords, types, and user words.
				auto add_if_match = [&](const std::string& candidate, Color color) {
					if (candidate.size() >= prefix.size() && candidate.starts_with(prefix)
				        && candidate != prefix) {
						completions.emplace_back(candidate, color);
					}
				};

				for (const auto& kw: DUCKLING_KEYWORDS) add_if_match(kw, Color::BRIGHTCYAN);
				for (const auto& tp: DUCKLING_TYPES) add_if_match(tp, Color::BRIGHTMAGENTA);
				for (const auto& uw: m_user_words) add_if_match(uw, Color::DEFAULT);

				std::ranges::sort(completions, [](const auto& lhs, const auto& rhs) {
					return lhs.text() < rhs.text();
				});

				return completions;
			}
		);
	}

	// ─── Hints ───────────────────────────────────────────────────────────────────

	void FrontendReplxxImplementation::setupHints() {
		m_replxx.set_hint_callback(
			[this](
				const std::string& input, int& context_len, replxx::Replxx::Color& color
			) -> replxx::Replxx::hints_t {
				replxx::Replxx::hints_t hints;
				std::string             prefix = extractWordEndingAt(input, input.size());
				if (prefix.empty()) return hints;

				context_len = static_cast<int>(prefix.size());
				color       = replxx::Replxx::Color::GRAY;

				auto add_hint = [&](const std::string& candidate) {
					if (candidate.size() > prefix.size() && candidate.starts_with(prefix))
						hints.push_back(candidate);
				};

				for (const auto& kw: DUCKLING_KEYWORDS) add_hint(kw);
				for (const auto& tp: DUCKLING_TYPES) add_hint(tp);
				for (const auto& uw: m_user_words) add_hint(uw);

				std::ranges::sort(hints);
				auto range = std::ranges::unique(hints);
				hints.erase(range.begin(), range.end());

				if (hints.size() == 1) color = replxx::Replxx::Color::GREEN;

				return hints;
			}
		);
	}

	// ─── Identifier collection ──────────────────────────────────────────────────

	void FrontendReplxxImplementation::collectIdentifiers(const std::string& input) {
		for (const auto& token: tokenizeIdentifiers(input)) {
			// Only collect user identifiers, not language keywords / types.
			if (token.size() >= 2 && (DUCKLING_KEYWORDS.count(token) == 0)
			    && (DUCKLING_TYPES.count(token) == 0)) {
				m_user_words.insert(token);
			}
		}
	}

	// ─── Welcome / prompt / help / history ──────────────────────────────────────

	void FrontendReplxxImplementation::printWelcome() const {
		std::cout << "Duckling REPL\n";
		std::cout << "Type /help for available commands, /exit to quit.\n";
		std::cout << "Press Enter to submit. Press Alt+Enter for new line.\n\n";
	}

	std::string FrontendReplxxImplementation::readLine() {
		const char* input = m_replxx.input(ReplConfig::PROMPT);

		if (input == nullptr) {
			// EOF (Ctrl-D) or error.
			std::cin.setstate(std::ios::eofbit);
			return {};
		}

		std::string line(input);

		if (!line.empty()) {
			m_replxx.history_add(line);
			collectIdentifiers(line);
		}

		return line;
	}

	void FrontendReplxxImplementation::printHistory() const {
		const int history_size = m_replxx.history_size();
		if (history_size <= 0) {
			std::cout << "No history yet.\n";
			return;
		}

		std::cout << "\n=== REPL History (" << history_size
				  << (history_size == 1 ? " entry" : " entries") << ") ===\n";

		auto  history_scan = m_replxx.history_scan();
		usize index        = 1;
		while (history_scan.next()) {
			const auto& entry = history_scan.get();
			std::cout << "[" << index++ << "] ";

			if (entry.text().find('\n') != std::string::npos) {
				std::cout << "(multiline)\n";
				std::cout << entry.text() << '\n';
			} else {
				std::cout << entry.text() << '\n';
			}
		}
		std::cout << '\n';
	}

	void FrontendReplxxImplementation::addHistoryEntry(std::string_view entry) {
		std::string owned(entry);
		if (owned.empty()) return;
		m_replxx.history_add(owned);
		collectIdentifiers(owned);
	}

	void FrontendReplxxImplementation::clearHistory() {
		m_replxx.history_clear();
		m_replxx.history_save(getHistoryFilePath());
	}

	void FrontendReplxxImplementation::clearScreen() { m_replxx.clear_screen(); }

	void FrontendReplxxImplementation::printHelp() const {
		printReplCommandsHelp(std::cout);
		std::cout << "\n=== Editing ===\n";
		std::cout << "  Enter               - Submit\n";
		std::cout << "  Alt + Enter         - Insert a new line\n";
		std::cout << "  Tab                 - Complete if available, else insert indentation\n";
		std::cout << "  Up / Down           - Navigate input history\n";
		std::cout << "  Ctrl+R              - Reverse search history\n";
		std::cout << "  Ctrl+D              - Exit (on empty line)\n";
		std::cout << '\n';
	}

}  // namespace compiler::repl
