#include "terminal.hpp"

#ifndef _WIN32
	#include <unistd.h>

	#include <iostream>

namespace os_utils {

	void writeStr(std::string_view str) { ::write(STDOUT_FILENO, str.data(), str.size()); }

	void writeChar(char c) { ::write(STDOUT_FILENO, &c, 1); }

	bool readChar(char& c) { return ::read(STDIN_FILENO, &c, 1) > 0; }

	void clearScreen() { writeStr("\033c\033[H\033[2J\033[0m"); }

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
}

#else
	#include <io.h>

	#include <iostream>

namespace os_utils {

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

	void clearScreen() {
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
			writeStr("\033c\033[H\033[2J\033[0m");
			return;
		}

		CONSOLE_SCREEN_BUFFER_INFO buffer_info{};
		if (!GetConsoleScreenBufferInfo(hOut, &buffer_info)) {
			writeStr("\033c\033[H\033[2J\033[0m");
			return;
		}

		const DWORD cells_count
			= static_cast<DWORD>(buffer_info.dwSize.X) * static_cast<DWORD>(buffer_info.dwSize.Y);
		const COORD home{ 0, 0 };
		DWORD       written = 0;

		if (!FillConsoleOutputCharacterA(hOut, ' ', cells_count, home, &written)) {
			writeStr("\033c\033[H\033[2J\033[0m");
			return;
		}

		if (!FillConsoleOutputAttribute(hOut, buffer_info.wAttributes, cells_count, home, &written)) {
			writeStr("\033c\033[H\033[2J\033[0m");
			return;
		}

		SetConsoleCursorPosition(hOut, home);
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
}

#endif
