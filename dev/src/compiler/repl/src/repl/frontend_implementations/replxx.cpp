#include "replxx.hpp"

#include <repl/helper_structs.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace compiler::repl {

	// ─── Duckling language keywords ──────────────────────────────────────────────

	// clang-format off
	static const std::set<std::string> DUCKLING_KEYWORDS = {
		"alias", "and", "as", "assert", "block", "box", "break", "case", "catch",
		"class", "compile_assert", "const", "continue", "copy", "debug", "defer",
		"dict", "else", "expand", "extends", "extern", "false", "for", "fun",
		"fundecl", "if", "implements", "import", "in", "lambda", "let", "loop",
		"match", "move", "namespace", "none", "not", "or", "pattern", "private",
		"protected", "public", "redo", "ref", "refof", "restart", "return", "set",
		"sizeof", "static", "str", "switch", "test", "then", "this", "throw",
		"true", "try", "type", "using", "var", "vec", "while", "with", "xor"
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
	static std::string extractWordEndingAt(const std::string& input, size_t pos) {
		if (pos == 0 || pos > input.length()) return "";

		size_t start = pos;
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

	// ─── History file path ───────────────────────────────────────────────────────

	std::string FrontendReplxxImplementation::getHistoryFilePath() {
		const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe)
		if (home != nullptr) return std::string(home) + "/.duckling_repl_history";
		return ".duckling_repl_history";
	}

	// ─── Construction / destruction ──────────────────────────────────────────────

	FrontendReplxxImplementation::FrontendReplxxImplementation() {
		m_replxx.set_max_history_size(1'000);
		m_replxx.set_word_break_characters(" \t\n;,+-/*%^&|~<>=!?@#$:(){}[]");
		m_replxx.set_indent_multiline(true);
		m_replxx.set_unique_history(true);
		m_replxx.set_double_tab_completion(false);
		m_replxx.set_complete_on_empty(false);
		m_replxx.set_beep_on_ambiguous_completion(false);
		m_replxx.set_max_hint_rows(8);
		m_replxx.set_hint_delay(0);

		// Load persistent history from file.
		m_replxx.history_load(getHistoryFilePath());

		setupKeyBindings();
		setupHighlighter();
		setupCompletion();
		setupHints();
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

		// ── Alt+Enter → newline ──
		m_replxx.bind_key_internal(Replxx::KEY::BASE_META | '\r', "new_line");
		m_replxx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::ENTER), "new_line");

		// ── F2 → new line (fallback for weird terminals) ──
		m_replxx.bind_key_internal(Replxx::KEY::F2, "new_line");
	}

	// ─── Syntax highlighting ─────────────────────────────────────────────────────

	void FrontendReplxxImplementation::setupHighlighter() {
		using Color = replxx::Replxx::Color;

		m_replxx.set_highlighter_callback([](const std::string&        input,
		                                     replxx::Replxx::colors_t& colors) {
			// We iterate over the input, identifying tokens and coloring them.
			size_t i   = 0;
			size_t len = input.size();

			while (i < len) {
				// Skip whitespace
				if (std::isspace(static_cast<unsigned char>(input[i]))) {
					++i;
					continue;
				}

				// String literal (double-quoted)
				if (input[i] == '"') {
					size_t start = i;
					++i;
					while (i < len && input[i] != '"') {
						if (input[i] == '\\' && i + 1 < len) ++i;  // skip escape
						++i;
					}
					if (i < len) ++i;  // closing quote
					for (size_t j = start; j < i && j < colors.size(); ++j)
						colors[j] = Color::BRIGHTGREEN;
					continue;
				}

				// String literal (single-quoted / char literal)
				if (input[i] == '\'') {
					size_t start = i;
					++i;
					while (i < len && input[i] != '\'') {
						if (input[i] == '\\' && i + 1 < len) ++i;
						++i;
					}
					if (i < len) ++i;
					for (size_t j = start; j < i && j < colors.size(); ++j)
						colors[j] = Color::BRIGHTGREEN;
					continue;
				}

				// Line comment (//)
				if (input[i] == '/' && i + 1 < len && input[i + 1] == '/') {
					for (size_t j = i; j < colors.size(); ++j) colors[j] = Color::GRAY;
					break;  // rest of line is comment
				}

				// Numeric literal
				if (std::isdigit(static_cast<unsigned char>(input[i]))
				    || (input[i] == '.' && i + 1 < len
				        && std::isdigit(static_cast<unsigned char>(input[i + 1])))) {
					size_t start = i;
					while (i < len
					       && (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '.'
					           || input[i] == '_'))
						++i;
					for (size_t j = start; j < i && j < colors.size(); ++j)
						colors[j] = Color::YELLOW;
					continue;
				}

				// Identifier or keyword
				if (std::isalpha(static_cast<unsigned char>(input[i])) || input[i] == '_') {
					size_t start = i;
					while (i < len
					       && (std::isalnum(static_cast<unsigned char>(input[i]))
					           || input[i] == '_'))
						++i;
					std::string word = input.substr(start, i - start);

					Color color = Color::DEFAULT;
					if (DUCKLING_KEYWORDS.count(word) != 0)
						color = Color::BRIGHTCYAN;
					else if (DUCKLING_TYPES.count(word) != 0)
						color = Color::BRIGHTMAGENTA;
					else if (word == "true" || word == "false" || word == "none")
						color = Color::YELLOW;

					for (size_t j = start; j < i && j < colors.size(); ++j) colors[j] = color;
					continue;
				}

				// REPL command (starts with /)
				if (input[i] == '/' && i == 0) {
					for (auto& color: colors) color = Color::BRIGHTBLUE;
					return;
				}

				// Operator characters
				if (std::string_view("+-*/%^&|~<>=!?@#$:.->.").find(input[i])
				    != std::string_view::npos) {
					colors[i] = Color::BROWN;
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

		auto   history_scan = m_replxx.history_scan();
		size_t index        = 1;
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

	void FrontendReplxxImplementation::clearHistory() { m_replxx.history_clear(); }

	void FrontendReplxxImplementation::clearScreen() { m_replxx.clear_screen(); }

	void FrontendReplxxImplementation::printHelp() const {
		std::cout << "\n=== REPL Commands ===\n";
		std::cout << "  /help, /?           - Show this help message\n";
		std::cout << "  /exit, /quit, /q    - Exit the REPL\n";
		std::cout << "  /history, /h        - Show all executed statements\n";
		std::cout << "  /clear, /c          - Clear terminal\n";
		std::cout << "\n=== Editing ===\n";
		std::cout << "  Enter               - Submit\n";
		std::cout << "  Alt + Enter         - Insert a new line\n";
		std::cout << "  Tab                 - Autocomplete keywords / identifiers\n";
		std::cout << "  Up / Down           - Navigate input history\n";
		std::cout << "  Ctrl+R              - Reverse search history\n";
		std::cout << "  Ctrl+D              - Exit (on empty line)\n";
		std::cout << '\n';
	}

}  // namespace compiler::repl
