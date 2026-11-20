// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#define NOMINMAX
#include "minimal.hpp"

#include <repl/helpers.hpp>

#include <base/types/ints.hpp>

#include <os_utils/terminal.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#define ESC "\x1b"

namespace {
	inline constexpr std::string_view CURSOR_LEFT_SEQ
		= ESC "[D";  // \x1b is start of ANSI escape sequence - needed to control terminal.
	inline constexpr std::string_view CURSOR_RIGHT_SEQ     = ESC "[C";
	inline constexpr std::string_view CURSOR_UP_SEQ        = ESC "[A";
	inline constexpr char             BACKSPACE_CHAR       = 0x7f;  // DEL
	inline constexpr char             NEWLINE_CHAR         = '\n';
	inline constexpr char             CARRIAGE_RETURN_CHAR = '\r';
	inline constexpr char             ESC_CHAR             = 0x1b;
	inline constexpr char             ARROW_SEQ_LEAD       = '[';
	inline constexpr char             ARROW_UP_CODE        = 'A';
	inline constexpr char             ARROW_DOWN_CODE      = 'B';
	inline constexpr char             ARROW_LEFT_CODE      = 'D';
	inline constexpr char             ARROW_RIGHT_CODE     = 'C';
	inline constexpr char             PRINTABLE_MIN        = 0x20;  // Space
	inline constexpr char             PRINTABLE_MAX        = 0x7e;  // ~

	// ANSI escape sequences for bracketed paste mode (https://en.wikipedia.org/wiki/Bracketed-paste)
	inline constexpr std::string_view ENABLE_BRACKETED_PASTE_SEQ  = ESC "[?2004h";
	inline constexpr std::string_view DISABLE_BRACKETED_PASTE_SEQ = ESC "[?2004l";
	inline constexpr char             BRACKETED_PASTE_PREFIX1     = '2';
	inline constexpr char             BRACKETED_PASTE_PREFIX2     = '0';
	inline constexpr char             BRACKETED_PASTE_START_CODE  = '0';
	inline constexpr char             BRACKETED_PASTE_END_CODE    = '1';
	inline constexpr char             BRACKETED_PASTE_SUFFIX      = '~';

	// ANSI escape sequences for modified keys (e.g. Ctrl/Alt + Arrow Keys)
	inline constexpr char EXTENDED_KEY_PREFIX    = '1';
	inline constexpr char EXTENDED_KEY_SEPARATOR = ';';
	inline constexpr char MODIFIER_CTRL          = '5';
	inline constexpr char MODIFIER_ALT           = '3';

	std::string concatLines(const std::vector<std::string>& lines, std::string_view new_line_indent) {
		std::string multiline_content;
		for (u64 i = 0; i < lines.size(); i++) {
			multiline_content += lines[i];
			if (i < lines.size() - 1) {
				multiline_content += NEWLINE_CHAR;
				multiline_content += new_line_indent;
			}
		}
		return multiline_content;
	}
}

namespace compiler::repl {
	void FrontendMinImplementation::EditorState::reset() {
		lines.clear();
		lines.emplace_back("");
		row            = 0;
		col            = 0;
		prev_state_row = 0;
		prev_state_col = 0;
		prev_state_lengths.clear();
		prev_state_lengths.push_back(0);
	}

	std::string FrontendMinImplementation::EditorState::print(bool for_history) const {
		return concatLines(
			lines,
			(for_history) ? compiler::repl::ReplConfig::HISTORY_MULTILINE_CONTINUATION
						  : compiler::repl::ReplConfig::CONTINUATION
		);
	}

	std::string FrontendMinImplementation::EditorState::contentFromRow(const u64 from_row) const {
		if (from_row >= lines.size()) return "";
		auto it = lines.begin();
		std::advance(it, static_cast<std::vector<std::string>::difference_type>(from_row));
		std::vector<std::string> sub_lines(it, lines.end());
		return concatLines(sub_lines, compiler::repl::ReplConfig::CONTINUATION);
	}

	std::string FrontendMinImplementation::EditorState::rawContent() const {
		return concatLines(lines, "");
	}

	FrontendMinImplementation::EditorState& FrontendMinImplementation::EditorState::operator=(
		const EditorState& other
	) {
		if (this != &other) {
			prev_state_row = row;
			prev_state_col = col;
			row            = other.row;
			col            = other.col;
			prev_state_lengths.clear();
			for (const auto& line: lines) prev_state_lengths.push_back(line.size());
			lines = other.lines;
		}
		return *this;
	}

	FrontendMinImplementation::FrontendMinImplementation(
		bool completions_enabled, bool bracketed_paste_enabled, bool decorative_output
	):
		  m_hist_idx(0),
		  m_sequence_to_align_cursor_to_multiline_start(
			  std::format("{}[{}C", ESC, ReplConfig::CONTINUATION.size())
		  ),
		  m_bracketed_paste_enabled(bracketed_paste_enabled),
		  m_decorative_output(decorative_output) {
		if (completions_enabled)
			std::cerr << "Warning: minimal REPL frontend does not support completions.\n";
	}

	void FrontendMinImplementation::printWelcome() const {
		if (!m_decorative_output) return;
		std::cout << "Duckling REPL (minimal mode)\n";
		std::cout << "Type /help for available commands, /exit to quit.\n\n";
	}

	std::string FrontendMinImplementation::readLine() {
		if (!m_decorative_output) {
			// Bypass terminal editing so it emits no redraw or cursor-control sequences.
			std::string line;
			char        c = 0;
			while (os_utils::readChar(c)) {
				if (c == NEWLINE_CHAR) {
					addHistoryEntry(line);
					return line;
				}
				if (c != CARRIAGE_RETURN_CHAR) line += c;
			}
			std::cin.setstate(std::ios::eofbit);
			if (!line.empty()) addHistoryEntry(line);
			return line;
		}

		std::cout << ReplConfig::PROMPT;
		std::cout.flush();

		if (m_bracketed_paste_enabled) os_utils::writeStr(ENABLE_BRACKETED_PASTE_SEQ);

		auto terminal_guard = os_utils::RawTerminalMode::create();
		if (!terminal_guard) throw std::runtime_error(terminal_guard.error());
		auto line = internalReadLine();

		if (m_bracketed_paste_enabled) os_utils::writeStr(DISABLE_BRACKETED_PASTE_SEQ);

		return line;
	}

	void FrontendMinImplementation::printHistory() const {
		if (m_history.empty()) {
			std::cout << "No history yet.\n";
			return;
		}

		std::cout << "\n=== REPL History (" << m_history.size()
				  << (m_history.size() == 1 ? " statement" : " statements") << ") ===\n";
		for (usize i = 0; i < m_history.size(); ++i)
			std::cout << "[" << (i + 1) << "] " << m_history[i].print(true) << '\n';
		std::cout << '\n';
	}

	void FrontendMinImplementation::addHistoryEntry(std::string_view entry) {
		EditorState state;
		state.reset();
		state.lines.clear();

		std::istringstream stream{ std::string(entry) };
		std::string        line;
		while (std::getline(stream, line)) state.lines.push_back(line);
		if (state.lines.empty()) state.lines.emplace_back("");

		state.row = state.lines.size() - 1;
		state.col = state.lines.back().size();
		state.prev_state_lengths.clear();
		for (const auto& saved_line: state.lines)
			state.prev_state_lengths.push_back(saved_line.size());
		state.prev_state_row = state.row;
		state.prev_state_col = state.col;

		m_history.push_back(state);
		m_hist_idx = m_history.size();
	}

	void FrontendMinImplementation::clearHistory() { m_history.clear(); }

	void FrontendMinImplementation::printHelp() const {
		printReplCommandsHelp(std::cout);
		std::cout << "\n=== Editing ===\n";
		std::cout << "  Alt + Enter         - Insert a new line\n";
		std::cout << "  Alt + Up / Down     - Navigate input history\n";
		std::cout << "  Ctrl+D              - Exit (on empty line)\n";
		std::cout << '\n';
	}

	void FrontendMinImplementation::printCompletions(std::string_view) const {
		std::cerr << "Completions are not supported by the minimal REPL frontend.\n";
	}

	void FrontendMinImplementation::moveCursorLeft() const { os_utils::writeStr(CURSOR_LEFT_SEQ); }

	void FrontendMinImplementation::moveCursorRight() const {
		os_utils::writeStr(CURSOR_RIGHT_SEQ);
	}

	void FrontendMinImplementation::moveCursorUp() const { os_utils::writeStr(CURSOR_UP_SEQ); }

	void FrontendMinImplementation::moveCursorFromEndToPos(u64 pos, std::string& line) const {
		for (u64 i = pos; i < line.size(); ++i) moveCursorLeft();
	}

	void FrontendMinImplementation::moveCursorToNewLine() const {
		os_utils::writeChar(NEWLINE_CHAR);
		os_utils::writeStr(ReplConfig::CONTINUATION);
	}

	std::string FrontendMinImplementation::internalReadLine() {
		m_editor_state.reset();
		m_hist_idx = m_history.size();
		std::cout.flush();

		while (true) {
			char c = 0;
			if (!os_utils::readChar(c)) break;
			if (c == ESC_CHAR) {
				onEscapeSequence();
			} else if (m_in_bracketed_paste) {
				if (c == NEWLINE_CHAR || c == CARRIAGE_RETURN_CHAR)
					onNewLine();
				else if (c >= PRINTABLE_MIN && c <= PRINTABLE_MAX)
					onPrintableChar(c);
			} else {
				if (c == NEWLINE_CHAR || c == CARRIAGE_RETURN_CHAR)  // Commit.
					break;
				else if (c == BACKSPACE_CHAR)
					onBackspace();
				else if (c >= PRINTABLE_MIN && c <= PRINTABLE_MAX)
					onPrintableChar(c);
			}

			if (!m_in_bracketed_paste) refreshScreen();
		}

		saveToHistory();
		os_utils::writeChar(NEWLINE_CHAR);
		return m_editor_state.rawContent();
	}

	void FrontendMinImplementation::saveToHistory() { m_history.push_back(m_editor_state); }

	void FrontendMinImplementation::onNewLine() {
		auto& row = m_editor_state.row;
		auto& col = m_editor_state.col;

		auto& line     = m_editor_state.lines[row];
		auto  new_line = line.substr(col);  // We are moving suffix to the new line.
		line.erase(col);                    // And removing it from the current line.
		auto new_line_it = m_editor_state.lines.begin();
		std::advance(new_line_it, static_cast<std::vector<std::string>::difference_type>(row + 1));
		m_editor_state.lines.insert(new_line_it, new_line);

		row++;
		col = 0;
	}

	void FrontendMinImplementation::onBackspace() {
		auto& row   = m_editor_state.row;
		auto& col   = m_editor_state.col;
		auto& lines = m_editor_state.lines;

		if (col > 0) {
			col--;
			m_editor_state.lines[m_editor_state.row].erase(col, 1);
		} else {
			if (row == 0) return;
			auto line_to_be_erased_it = lines.begin();
			std::advance(
				line_to_be_erased_it, static_cast<std::vector<std::string>::difference_type>(row)
			);
			auto to_be_appended = lines[row];
			row--;
			lines.erase(line_to_be_erased_it);
			col = lines[row].size();
			lines[row] += to_be_appended;
		}
	}

	void FrontendMinImplementation::onEscapeSequence() {
		char seq1 = 0;
		if (!os_utils::readChar(seq1)) return;
		if (seq1 == NEWLINE_CHAR || seq1 == CARRIAGE_RETURN_CHAR) {  // Activated on ALT + ENTER
			onNewLine();
		} else if (seq1 == ARROW_SEQ_LEAD) {
			char seq2 = 0;
			if (!os_utils::readChar(seq2)) return;
			if (seq2 == ARROW_UP_CODE)
				onArrowUp();
			else if (seq2 == ARROW_DOWN_CODE)
				onArrowDown();
			else if (seq2 == ARROW_LEFT_CODE)
				onArrowLeft();
			else if (seq2 == ARROW_RIGHT_CODE)
				onArrowRight();
			else if (seq2 == BRACKETED_PASTE_PREFIX1) {
				char seq3 = 0, seq4 = 0, seq5 = 0;
				if (!os_utils::readChar(seq3)) return;
				if (seq3 == BRACKETED_PASTE_PREFIX2) {
					if (!os_utils::readChar(seq4)) return;
					if (seq4 == BRACKETED_PASTE_START_CODE) {
						if (!os_utils::readChar(seq5)) return;
						if (seq5 == BRACKETED_PASTE_SUFFIX) m_in_bracketed_paste = true;
					} else if (seq4 == BRACKETED_PASTE_END_CODE) {
						if (!os_utils::readChar(seq5)) return;
						if (seq5 == BRACKETED_PASTE_SUFFIX) m_in_bracketed_paste = false;
					}
				}
			} else if (seq2 == EXTENDED_KEY_PREFIX) {
				char seq3 = 0, seq4 = 0, seq5 = 0;
				if (!os_utils::readChar(seq3)) return;
				if (seq3 == EXTENDED_KEY_SEPARATOR) {
					if (!os_utils::readChar(seq4)) return;
					if (!os_utils::readChar(seq5)) return;
					if (seq4 == MODIFIER_CTRL
					    || seq4 == MODIFIER_ALT) {  // Alt or Ctrl - depends on the terminal.
						if (seq5 == ARROW_UP_CODE)
							historyScrollUp();
						else if (seq5 == ARROW_DOWN_CODE)
							historyScrollDown();
					}
				}
			}
		}
	}

	void FrontendMinImplementation::onPrintableChar(char c) {
		m_editor_state.lines[m_editor_state.row].insert(m_editor_state.col, 1, c);
		m_editor_state.col++;
	}

	void FrontendMinImplementation::onArrowLeft() {
		if (m_editor_state.col > 0) m_editor_state.col--;
	}

	void FrontendMinImplementation::onArrowRight() {
		if (m_editor_state.col < m_editor_state.lines[m_editor_state.row].size())
			m_editor_state.col++;
	}

	void FrontendMinImplementation::historyScrollUp() {
		if (m_hist_idx > 0) {
			if (m_hist_idx == m_history.size()) m_stashed_editor_state = m_editor_state;
			if (m_hist_idx < m_history.size()) {
				m_history[m_hist_idx].row = m_editor_state.row;
				m_history[m_hist_idx].col = m_editor_state.col;
			}
			m_hist_idx--;
			m_editor_state = m_history[m_hist_idx];
		}
	}

	void FrontendMinImplementation::historyScrollDown() {
		if (m_hist_idx < m_history.size()) {
			m_history[m_hist_idx].row = m_editor_state.row;
			m_history[m_hist_idx].col = m_editor_state.col;
			m_hist_idx++;
			if (m_hist_idx != m_history.size())
				m_editor_state = m_history[m_hist_idx];
			else
				m_editor_state = m_stashed_editor_state;
		}
	}

	void FrontendMinImplementation::onArrowUp() {
		if (m_editor_state.row > 0) {
			m_editor_state.row--;
			m_editor_state.col
				= std::min(m_editor_state.col, m_editor_state.lines[m_editor_state.row].size());
		}
	}

	void FrontendMinImplementation::onArrowDown() {
		if (m_editor_state.row < m_editor_state.lines.size() - 1) {
			m_editor_state.row++;
			m_editor_state.col
				= std::min(m_editor_state.col, m_editor_state.lines[m_editor_state.row].size());
		}
	}

	std::string FrontendMinImplementation::sequenceToMoveCursorFromGivenPosToStart(u64 row, u64 col) {
		std::string seq;
		for (u64 i = 0; i < row; i++) seq += CURSOR_UP_SEQ;
		for (u64 i = 0; i < col; i++) seq += CURSOR_LEFT_SEQ;
		return seq;
	}

	std::string FrontendMinImplementation::sequenceToMoveCursorFromEndToCurrentPosition() {
		auto&       col   = m_editor_state.col;
		auto&       lines = m_editor_state.lines;
		std::string seq;
		u64         num_of_lines_to_go_up = lines.size() - m_editor_state.row - 1;
		for (u64 i = 0; i < num_of_lines_to_go_up; i++) seq += CURSOR_UP_SEQ;

		auto size_of_last_line = lines.back().size();
		if (col <= size_of_last_line)
			for (u64 i = 0; i < size_of_last_line - col; i++) seq += CURSOR_LEFT_SEQ;
		else
			for (u64 i = 0; i < col - size_of_last_line; i++) seq += CURSOR_RIGHT_SEQ;

		return seq;
	}

	std::string FrontendMinImplementation::clearScreenSequence() {
		std::string clear_seq;
		auto&       prev_st_lengths = m_editor_state.prev_state_lengths;
		clear_seq += sequenceToMoveCursorFromGivenPosToStart(
			m_editor_state.prev_state_row, m_editor_state.prev_state_col
		);

		for (usize i = 0; i < prev_st_lengths.size(); i++) {
			clear_seq += std::string(prev_st_lengths[i], ' ');
			if (i < prev_st_lengths.size() - 1) {
				clear_seq += NEWLINE_CHAR;
				clear_seq += ReplConfig::CONTINUATION;
			}
		}

		for (u64 i = 0; i < prev_st_lengths.back(); i++) clear_seq += CURSOR_LEFT_SEQ;
		for (u64 i = 0; i < prev_st_lengths.size() - 1; i++) clear_seq += CURSOR_UP_SEQ;

		return clear_seq;
	}

	void FrontendMinImplementation::refreshScreen() {
		std::string buffered_sequence_to_send = clearScreenSequence();
		buffered_sequence_to_send += m_editor_state.print();
		buffered_sequence_to_send += sequenceToMoveCursorFromEndToCurrentPosition();
		os_utils::writeStr(buffered_sequence_to_send);

		updatePrevState();
	}

	void FrontendMinImplementation::updatePrevState() {
		m_editor_state.prev_state_lengths.clear();
		for (auto& line: m_editor_state.lines)
			m_editor_state.prev_state_lengths.push_back(line.size());

		m_editor_state.prev_state_row = m_editor_state.row;
		m_editor_state.prev_state_col = m_editor_state.col;
	}

	void FrontendMinImplementation::clearScreen() { os_utils::clearScreen(); }
}  // namespace compiler::repl
