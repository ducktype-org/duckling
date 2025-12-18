#include "repl_frontend.hpp"

#include <termios.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	constexpr std::string_view CURSOR_LEFT_SEQ      = "\x1b[D";
	constexpr std::string_view CURSOR_RIGHT_SEQ     = "\x1b[C";
	constexpr std::string_view CURSOR_DOWN_SEQ      = "\x1b[B";
	constexpr std::string_view CURSOR_UP_SEQ        = "\x1b[A";
	constexpr char             BACKSPACE_CHAR       = 0x7f;  // DEL
	constexpr char             NEWLINE_CHAR         = '\n';
	constexpr char             CARRIAGE_RETURN_CHAR = '\r';
	constexpr char             ESC_CHAR             = 0x1b;
	constexpr char             ARROW_SEQ_LEAD       = '[';
	constexpr char             ARROW_UP_CODE        = 'A';
	constexpr char             ARROW_DOWN_CODE      = 'B';
	constexpr char             ARROW_LEFT_CODE      = 'D';
	constexpr char             ARROW_RIGHT_CODE     = 'C';
	constexpr std::string_view ERASE_SEQ            = "\b \b";  // backspace, space, backspace
	constexpr char             PRINTABLE_MIN        = 0x20;     // Space
	constexpr char             PRINTABLE_MAX        = 0x7e;     // ~
}

namespace {
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
			std::cin.setstate(std::ios::eofbit);
		}
	}
}

namespace compiler::repl {
	void ReplFrontend::writeStr(std::string_view str) {
		::write(STDOUT_FILENO, str.data(), str.size());
	}

	void ReplFrontend::writeStr(std::string_view str, size_t from_pos) {
		if (from_pos > str.size()) std::cerr << "ReplFrontend::writeStr: from_pos out of range\n";
		writeStr(str.substr(from_pos));
	}

	void ReplFrontend::writeChar(char c) { ::write(STDOUT_FILENO, &c, 1); }

	void ReplFrontend::printWelcome() const {
		std::cout << "Duckling REPL\n";
		std::cout << "Type /help for available commands, /exit to quit.\n";
		std::cout << "Enter " << m_config.multiline_start << " for multiline mode.\n\n";
	}

	void ReplFrontend::printPrompt() const {
		std::cout << m_config.prompt;
		std::cout.flush();
	}

	void ReplFrontend::moveCursorLeft() const { writeStr(CURSOR_LEFT_SEQ); }

	void ReplFrontend::moveCursorRight() const { writeStr(CURSOR_RIGHT_SEQ); }

	void ReplFrontend::moveCursorFromEndToPos(size_t pos, std::string& buffer) {
		for (size_t i = pos; i < buffer.size(); ++i) moveCursorLeft();
	}

	void ReplFrontend::moveCursorToEnd() {
		while (m_cursor_pos < m_buffer.size()) {
			moveCursorRight();
			m_cursor_pos++;
		}
	}

	void ReplFrontend::clearBuffer() {
		m_buffer.clear();
		m_cursor_pos = 0;
	}

	void ReplFrontend::singleLineOnBackspace() {
		if (m_cursor_pos > 0) {
			moveCursorLeft();
			m_cursor_pos--;
			m_buffer.erase(m_cursor_pos, 1);
			writeStr(m_buffer, m_cursor_pos);
			writeStr(" ");
			moveCursorLeft();
			moveCursorFromEndToPos(m_cursor_pos, m_buffer);
		}
	}

	void ReplFrontend::singleLineOnEscapeSequence() {
		// Escape sequence; try to read two more bytes to determine the sequence.
		std::array<char, 2> seq = { 0, 0 };
		if (::read(STDIN_FILENO, &seq[0], 1) <= 0) return;
		if (::read(STDIN_FILENO, &seq[1], 1) <= 0) return;
		if (seq[0] == ARROW_SEQ_LEAD) {
			if (seq[1] == ARROW_UP_CODE) {
				// Up.
				if (m_hist_idx > 0) {
					// Find previous single-line history item
					ssize_t search_idx = m_hist_idx - 1;
					while (search_idx >= 0) {
						if (m_history[static_cast<size_t>(search_idx)].source_code.find('\n')
						    == std::string::npos) {
							break;
						}
						search_idx--;
					}

					if (search_idx >= 0) {
						m_hist_idx = search_idx;
						moveCursorToEnd();
						// Clear current buffer from terminal.
						for (size_t i = 0; i < m_buffer.size(); ++i) writeStr(ERASE_SEQ);
						m_buffer     = m_history[static_cast<size_t>(m_hist_idx)].source_code;
						m_cursor_pos = m_buffer.size();
						writeStr(m_buffer);
					}
				}
			} else if (seq[1] == ARROW_DOWN_CODE) {
				if (m_hist_idx < static_cast<ssize_t>(m_history.size())) {
					// Find next single-line history item
					ssize_t search_idx = m_hist_idx + 1;
					while (search_idx < static_cast<ssize_t>(m_history.size())) {
						if (m_history[static_cast<size_t>(search_idx)].source_code.find('\n')
						    == std::string::npos) {
							break;
						}
						search_idx++;
					}

					if (search_idx <= static_cast<ssize_t>(m_history.size())) {
						m_hist_idx = search_idx;
						moveCursorToEnd();
						// Clear.
						for (size_t i = 0; i < m_buffer.size(); ++i) writeStr(ERASE_SEQ);

						if (m_hist_idx == static_cast<ssize_t>(m_history.size())) {
							clearBuffer();
						} else {
							m_buffer     = m_history[static_cast<size_t>(m_hist_idx)].source_code;
							m_cursor_pos = m_buffer.size();
							writeStr(m_buffer);
						}
					}
				}
			} else if (seq[1] == ARROW_LEFT_CODE) {
				if (m_cursor_pos > 0) {
					m_cursor_pos--;
					moveCursorLeft();
				}
			} else if (seq[1] == ARROW_RIGHT_CODE) {
				if (m_cursor_pos < m_buffer.size()) {
					m_cursor_pos++;
					moveCursorRight();
				}
			}
		}
	}

	void ReplFrontend::singleLineOnPrintableChar(char c) {
		m_buffer.insert(m_cursor_pos, 1, c);
		m_cursor_pos++;
		writeChar(c);
		if (m_cursor_pos < m_buffer.size()) {
			writeStr(m_buffer, m_cursor_pos);
			moveCursorFromEndToPos(m_cursor_pos, m_buffer);
		}
	}

	// Simple line reader that supports Up/Down arrow history navigation and
	// basic editing (backspace). It uses termios maybe Jakub finds something better.
	// TODO this should be in frontend.
	std::string ReplFrontend::readLine() {
		auto read_line_helper = [&]() -> std::string {
			try {
				RawTerminalMode terminal_guard;
				return handleSingleLineInput();
			} catch (const std::runtime_error& e) {
				// Fallback to getline if raw mode setup fails
				std::string line;
				std::getline(std::cin, line);
				return line;
			}
		};

		std::string line = read_line_helper();

		if (line == m_config.multiline_start) {
			try {
				RawTerminalMode terminal_guard;
				return handleMultilineInput();
			} catch (const std::runtime_error& e) {
				std::cerr << e.what() << '\n';
				return "";
			}
		}

		return line;
	}

	std::string ReplFrontend::handleSingleLineInput() {
		clearBuffer();
		m_hist_idx = static_cast<ssize_t>(m_history.size());

		while (true) {
			char    c = 0;
			ssize_t r = ::read(STDIN_FILENO, &c, 1);
			if (r <= 0) {
				// EOF or error.
				return {};
			} else if (c == NEWLINE_CHAR || c == CARRIAGE_RETURN_CHAR) {
				// Enter.
				writeChar(NEWLINE_CHAR);
				break;
			} else if (c == BACKSPACE_CHAR) {
				singleLineOnBackspace();
			} else if (c == ESC_CHAR) {
				singleLineOnEscapeSequence();
			} else if (c >= PRINTABLE_MIN && c <= PRINTABLE_MAX) {
				singleLineOnPrintableChar(c);
			}
		}
		return m_buffer;
	}

	bool ReplFrontend::multiLineOnNewLine(std::vector<std::string>& lines, size_t& row, size_t& col) {
		if (lines[row] == m_config.multiline_end) {
			lines.pop_back();
			writeChar(NEWLINE_CHAR);
			return true;
		}

		if (row == lines.size() - 1) {
			lines.push_back("");
			row++;
			col = 0;
			writeChar(NEWLINE_CHAR);
			writeStr(m_config.continuation);
		} else {
			row++;
			col = 0;
			writeStr(CURSOR_DOWN_SEQ);
			writeChar(CARRIAGE_RETURN_CHAR);                   // Start of line
			writeStr(m_cursor_align_to_multiline_prompt_end);  // Move cursor beyond the prompt
		}
		return false;
	}

	void ReplFrontend::multiLineOnBackspace(
		std::vector<std::string>& lines, size_t& row, size_t& col
	) {
		if (col > 0) {
			col--;
			lines[row].erase(col, 1);
			moveCursorLeft();
			writeStr(lines[row], col);
			writeStr(" ");
			moveCursorLeft();
			moveCursorFromEndToPos(col, lines[row]);
		}
	}

	void ReplFrontend::multiLineOnEscapeSequence(
		std::vector<std::string>& lines, size_t& row, size_t& col
	) {
		std::array<char, 2> seq = { 0, 0 };
		if (::read(STDIN_FILENO, &seq[0], 1) <= 0) return;
		if (::read(STDIN_FILENO, &seq[1], 1) <= 0) return;

		if (seq[0] == ARROW_SEQ_LEAD) {
			if (seq[1] == ARROW_UP_CODE) {
				if (row > 0) {
					row--;
					size_t old_col = col;
					col            = std::min(col, lines[row].size());
					writeStr(CURSOR_UP_SEQ);

					// Adjust horizontal position
					if (col > old_col)
						for (size_t i = 0; i < col - old_col; ++i) moveCursorRight();
					else if (col < old_col)
						for (size_t i = 0; i < old_col - col; ++i) moveCursorLeft();
				}
			} else if (seq[1] == ARROW_DOWN_CODE) {
				if (row < lines.size() - 1) {
					row++;
					size_t old_col = col;
					col            = std::min(col, lines[row].size());
					writeStr(CURSOR_DOWN_SEQ);

					if (col > old_col)
						for (size_t i = 0; i < col - old_col; ++i) moveCursorRight();
					else if (col < old_col)
						for (size_t i = 0; i < old_col - col; ++i) moveCursorLeft();
				}
			} else if (seq[1] == ARROW_LEFT_CODE) {
				if (col > 0) {
					col--;
					moveCursorLeft();
				}
			} else if (seq[1] == ARROW_RIGHT_CODE) {
				if (col < lines[row].size()) {
					col++;
					moveCursorRight();
				}
			}
		}
	}

	void ReplFrontend::multiLineOnPrintableChar(
		char c, std::vector<std::string>& lines, size_t& row, size_t& col
	) {
		lines[row].insert(col, 1, c);
		col++;
		writeChar(c);
		if (col < lines[row].size()) {
			writeStr(lines[row], col);
			moveCursorFromEndToPos(col, lines[row]);
		}
	}

	std::string ReplFrontend::handleMultilineInput() {
		std::vector<std::string> lines;
		lines.push_back("");
		size_t row = 0;
		size_t col = 0;

		std::cout << "(Multiline mode - type '" << m_config.multiline_end
				  << "' on a new line to finish)\n";
		std::cout << m_config.continuation;
		std::cout.flush();

		while (true) {
			char c = 0;
			if (::read(STDIN_FILENO, &c, 1) <= 0) break;
			if (c == NEWLINE_CHAR || c == CARRIAGE_RETURN_CHAR) {
				auto end = multiLineOnNewLine(lines, row, col);
				if (end) break;
			} else if (c == BACKSPACE_CHAR) {
				multiLineOnBackspace(lines, row, col);
			} else if (c == ESC_CHAR) {
				multiLineOnEscapeSequence(lines, row, col);
			} else if (c >= PRINTABLE_MIN && c <= PRINTABLE_MAX) {
				multiLineOnPrintableChar(c, lines, row, col);
			}
		}

		std::string multiline_content;
		for (size_t i = 0; i < lines.size(); ++i) {
			if (i > 0) multiline_content += NEWLINE_CHAR;
			multiline_content += lines[i];
		}
		return multiline_content;
	}

	void ReplFrontend::printHistory() const {
		if (m_history.empty()) {
			std::cout << "No history yet.\n";
			return;
		}

		std::cout << "\n=== REPL History (" << m_history.size()
				  << (m_history.size() == 1 ? " statement" : " statements") << ") ===\n";
		for (size_t i = 0; i < m_history.size(); ++i) {
			const auto& stmt = m_history[i];
			std::cout << "[" << (i + 1) << "] ";

			if (stmt.source_code.find('\n') != std::string::npos) {
				std::cout << "(multiline)\n";
				std::cout << stmt.source_code << NEWLINE_CHAR;
			} else {
				std::cout << stmt.source_code << NEWLINE_CHAR;
			}
		}
		std::cout << NEWLINE_CHAR;
	}

	void ReplFrontend::printHelp() const {
		std::cout << "\n=== REPL Commands ===\n";
		std::cout << "  /help, /?           - Show this help message\n";
		std::cout << "  /exit, /quit, /q    - Exit the REPL\n";
		std::cout << "  /history, /h        - Show all executed statements\n";
		std::cout << "  /clear, /c          - Clear statement history\n";
		std::cout << "\n=== Multiline Mode ===\n";
		std::cout << "  " << m_config.multiline_start
				  << "                  - Start multiline input\n";
		std::cout << "  " << m_config.multiline_end
				  << "                   - End multiline input and execute\n";
		std::cout << NEWLINE_CHAR;
	}
}
