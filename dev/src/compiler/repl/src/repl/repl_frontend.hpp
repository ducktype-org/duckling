#pragma once

#include "repl_structs.hpp"

#include <termios.h>

#include <cstddef>
#include <string>
#include <vector>

/**
 * @brief RawTerminalMode
 *
 * Changes the operating mode of the terminal. Uses RAII to ensure terminal settings are untouched
 * upon exiting repl.
 */
namespace {
	class RawTerminalMode {
	public:
		RawTerminalMode();
		~RawTerminalMode();

	private:
		struct termios m_orig_term{};
		bool           m_raw_mode_set = false;
	};
}

namespace compiler::repl {

	/**
	 * @brief Frontend interface for REPL session.
	 *
	 * Handles interactions in console.
	 */
	class ReplFrontend {
	public:
		ReplFrontend(std::vector<ReplStatement>& history, const ReplConfig& config):
			  m_history(history),
			  m_config(config),
			  m_cursor_align_to_multiline_prompt_end(
				  std::format(
					  "\x1b[{}C", config.continuation.size()
				  )  // \x1b is start of ANSI escape sequence - needed to control terminal
			  ) {}

		void printWelcome() const;
		// Read a single line from terminal with simple history navigation (up/down arrows).
		// This implements a small in-process line editor so REPL can navigate previous
		// inputs without depending on readline/libedit.
		std::string readLine();
		void        printPrompt() const;
		void        printHistory() const;
		void        printHelp() const;

	private:
		std::string handleSingleLineInput();
		std::string handleMultilineInput();
		void        moveCursorLeft() const;
		void        moveCursorRight() const;
		void        moveCursorFromEndToPos(size_t pos, std::string& buffer);
		void        moveCursorToEnd();

		static void writeStr(std::string_view str);
		static void writeStr(std::string_view str, size_t from_pos);
		static void writeChar(char c);

		void clearBuffer();
		void singleLineOnBackspace();
		void singleLineOnEscapeSequence();
		void singleLineOnPrintableChar(char c);

		bool multiLineOnNewLine(std::vector<std::string>& lines, size_t& row, size_t& col);
		void multiLineOnBackspace(std::vector<std::string>& lines, size_t& row, size_t& col);
		void multiLineOnEscapeSequence(std::vector<std::string>& lines, size_t& row, size_t& col);
		void multiLineOnPrintableChar(
			char c, std::vector<std::string>& lines, size_t& row, size_t& col
		);

		std::string                 m_buffer;
		size_t                      m_cursor_pos = 0;
		ssize_t                     m_hist_idx   = 0;
		std::vector<ReplStatement>& m_history;
		const ReplConfig&           m_config;
		const std::string           m_cursor_align_to_multiline_prompt_end;
	};  // class ReplFrontend

}  // namespace compiler::repl
