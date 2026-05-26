#define NOMINMAX
#include "minimal.hpp"

#include <repl/helpers.hpp>

#include <base/types/ints.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#ifndef _WIN32
	#include <unistd.h>
#else
	#include <io.h>
#endif

#define ESC "\x1b"

namespace {
	inline constexpr std::string_view CURSOR_LEFT_SEQ
		= ESC "[D";  // \x1b is start of ANSI escape sequence - needed to control terminal.
	inline constexpr std::string_view CURSOR_RIGHT_SEQ        = ESC "[C";
	inline constexpr std::string_view CURSOR_UP_SEQ           = ESC "[A";
	inline constexpr char             BACKSPACE_CHAR          = 0x7f;  // DEL
	inline constexpr char             NEWLINE_CHAR            = '\n';
	inline constexpr char             CARRIAGE_RETURN_CHAR    = '\r';
	inline constexpr char             ESC_CHAR                = 0x1b;
	inline constexpr char             ARROW_SEQ_LEAD          = '[';
	inline constexpr char             ARROW_UP_CODE           = 'A';
	inline constexpr char             ARROW_DOWN_CODE         = 'B';
	inline constexpr char             ARROW_LEFT_CODE         = 'D';
	inline constexpr char             ARROW_RIGHT_CODE        = 'C';
	inline constexpr char             PRINTABLE_MIN           = 0x20;  // Space
	inline constexpr char             PRINTABLE_MAX           = 0x7e;  // ~
	inline constexpr std::string_view CLEAR_ENTIRE_SCREEN_SEQ = "\033c\033[H\033[2J\033[0m";

#ifndef _WIN32
	void writeStr(std::string_view str) { ::write(STDOUT_FILENO, str.data(), str.size()); }

	void writeChar(char c) { ::write(STDOUT_FILENO, &c, 1); }

	bool readChar(char& c) { return ::read(STDIN_FILENO, &c, 1) > 0; }

	RawTerminalMode::RawTerminalMode() {
		// Save original terminal settings.
		if (tcgetattr(STDIN_FILENO, &m_orig_term) == -1)
			throw std::runtime_error("Failed to get terminal attributes");

		m_raw_mode_set = true;
		auto new_term  = m_orig_term;
		auto flags_mask
			= static_cast<tcflag_t>(static_cast<tcflag_t>(ECHO) | static_cast<tcflag_t>(ICANON));
		new_term.c_lflag &= static_cast<tcflag_t>(~flags_mask);
		new_term.c_cc[VMIN]  = 1;
		new_term.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1)
			throw std::runtime_error("Failed to set raw terminal mode");
	}

	RawTerminalMode::~RawTerminalMode() {
		if (m_raw_mode_set) {
			tcsetattr(STDIN_FILENO, TCSANOW, &m_orig_term);

			tcflush(STDIN_FILENO, TCIFLUSH);
			std::cin.sync();
			std::cin.clear();
		}
	}
#else
	void writeStr(std::string_view str) {
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD  written;
		WriteFile(hOut, str.data(), static_cast<DWORD>(str.size()), &written, NULL);
	}

	void writeChar(char c) {
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD  written;
		WriteFile(hOut, &c, 1, &written, NULL);
	}

	bool readChar(char& c) {
		HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
		DWORD  read;
		return ReadFile(hIn, &c, 1, &read, NULL) && read > 0;
	}

	RawTerminalMode::RawTerminalMode() {
		HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) return;

		GetConsoleMode(hIn, &m_orig_in_mode);
		GetConsoleMode(hOut, &m_orig_out_mode);

		// Added ENABLE_VIRTUAL_TERMINAL_INPUT here
		DWORD new_in_mode = m_orig_in_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
		new_in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

		SetConsoleMode(hIn, new_in_mode);

		DWORD new_out_mode
			= m_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
		SetConsoleMode(hOut, new_out_mode);

		m_raw_mode_set = true;
	}

	RawTerminalMode::~RawTerminalMode() {
		if (m_raw_mode_set) {
			HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleMode(hIn, m_orig_in_mode);
			SetConsoleMode(hOut, m_orig_out_mode);

			FlushConsoleInputBuffer(hIn);
			std::cin.sync();
			std::cin.clear();
		}
	}
#endif

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

	FrontendMinImplementation::FrontendMinImplementation(bool completions_enabled):
		  m_hist_idx(0),
		  m_sequence_to_align_cursor_to_multiline_start(
			  std::format("{}[{}C", ESC, ReplConfig::CONTINUATION.size())
		  ) {
		if (completions_enabled)
			std::cerr << "Warning: minimal REPL frontend does not support completions.\n";
	}

	void FrontendMinImplementation::printWelcome() const {
		std::cout << "Duckling REPL (minimal mode)\n";
		std::cout << "Type /help for available commands, /exit to quit.\n\n";
	}

	std::string FrontendMinImplementation::readLine() {
		std::cout << ReplConfig::PROMPT;

		RawTerminalMode terminal_guard;
		auto            line = internalReadLine();

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

	void FrontendMinImplementation::moveCursorLeft() const { writeStr(CURSOR_LEFT_SEQ); }

	void FrontendMinImplementation::moveCursorRight() const { writeStr(CURSOR_RIGHT_SEQ); }

	void FrontendMinImplementation::moveCursorUp() const { writeStr(CURSOR_UP_SEQ); }

	void FrontendMinImplementation::moveCursorFromEndToPos(u64 pos, std::string& line) const {
		for (u64 i = pos; i < line.size(); ++i) moveCursorLeft();
	}

	void FrontendMinImplementation::moveCursorToNewLine() const {
		writeChar(NEWLINE_CHAR);
		writeStr(ReplConfig::CONTINUATION);
	}

	std::string FrontendMinImplementation::internalReadLine() {
		m_editor_state.reset();
		m_hist_idx = m_history.size();
		std::cout.flush();

		while (true) {
			char c = 0;
			if (!readChar(c)) break;
			if (c == NEWLINE_CHAR || c == CARRIAGE_RETURN_CHAR)  // Commit.
				break;
			else if (c == BACKSPACE_CHAR)
				onBackspace();
			else if (c == ESC_CHAR)
				onEscapeSequence();
			else if (c >= PRINTABLE_MIN && c <= PRINTABLE_MAX)
				onPrintableChar(c);

			refreshScreen();
		}

		saveToHistory();
		writeChar(NEWLINE_CHAR);
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
		if (!readChar(seq1)) return;
		if (seq1 == NEWLINE_CHAR || seq1 == CARRIAGE_RETURN_CHAR) {  // Activated on ALT + ENTER
			onNewLine();
		} else if (seq1 == ARROW_SEQ_LEAD) {
			char seq2 = 0;
			if (!readChar(seq2)) return;
			if (seq2 == ARROW_UP_CODE)
				onArrowUp();
			else if (seq2 == ARROW_DOWN_CODE)
				onArrowDown();
			else if (seq2 == ARROW_LEFT_CODE)
				onArrowLeft();
			else if (seq2 == ARROW_RIGHT_CODE)
				onArrowRight();
			else if (seq2 == '1') {
				char seq3 = 0, seq4 = 0, seq5 = 0;
				if (!readChar(seq3)) return;
				if (seq3 == ';') {
					if (!readChar(seq4)) return;
					if (!readChar(seq5)) return;
					if (seq4 == '5' || seq4 == '3') {  // Alt or Ctrl - depends on the terminal.
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
		writeStr(buffered_sequence_to_send);

		updatePrevState();
	}

	void FrontendMinImplementation::updatePrevState() {
		m_editor_state.prev_state_lengths.clear();
		for (auto& line: m_editor_state.lines)
			m_editor_state.prev_state_lengths.push_back(line.size());

		m_editor_state.prev_state_row = m_editor_state.row;
		m_editor_state.prev_state_col = m_editor_state.col;
	}

	void FrontendMinImplementation::clearScreen() {
#ifndef _WIN32
		writeStr(CLEAR_ENTIRE_SCREEN_SEQ);
#else
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
			writeStr(CLEAR_ENTIRE_SCREEN_SEQ);
			return;
		}

		CONSOLE_SCREEN_BUFFER_INFO buffer_info{};
		if (!GetConsoleScreenBufferInfo(hOut, &buffer_info)) {
			writeStr(CLEAR_ENTIRE_SCREEN_SEQ);
			return;
		}

		const DWORD cells_count
			= static_cast<DWORD>(buffer_info.dwSize.X) * static_cast<DWORD>(buffer_info.dwSize.Y);
		const COORD home{ 0, 0 };
		DWORD       written = 0;

		if (!FillConsoleOutputCharacterA(hOut, ' ', cells_count, home, &written)) {
			writeStr(CLEAR_ENTIRE_SCREEN_SEQ);
			return;
		}

		if (!FillConsoleOutputAttribute(hOut, buffer_info.wAttributes, cells_count, home, &written)) {
			writeStr(CLEAR_ENTIRE_SCREEN_SEQ);
			return;
		}

		SetConsoleCursorPosition(hOut, home);
#endif
	}
}  // namespace compiler::repl
