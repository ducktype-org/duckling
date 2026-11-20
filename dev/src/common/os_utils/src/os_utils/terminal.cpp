// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "terminal.hpp"

#include <base/config/build_type.hpp>
#include <base/config/target_info.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <cerrno>
#include <cstring>
#include <iostream>

namespace {
	// ANSI clear-screen sequence; also the Windows fallback.
	constexpr std::string_view CLEAR_SCREEN_SEQUENCE = "\033c\033[H\033[2J\033[0m";
}

#if BASE_TARGET_PLATFORM_POSIX
	#include <unistd.h>

namespace os_utils {

	void writeStr(std::string_view str) { ::write(STDOUT_FILENO, str.data(), str.size()); }

	void writeChar(char c) { ::write(STDOUT_FILENO, &c, 1); }

	bool readChar(char& c) { return ::read(STDIN_FILENO, &c, 1) > 0; }

	void clearScreen() { writeStr(CLEAR_SCREEN_SEQUENCE); }

	RawTerminalMode::RawTerminalMode(RawTerminalMode&& other) noexcept:
		  m_orig_term{ other.m_orig_term },
		  m_raw_mode_set{ other.m_raw_mode_set } {
		other.m_raw_mode_set = false;
	}

	RawTerminalMode& RawTerminalMode::operator=(RawTerminalMode&& other) noexcept {
		if (this != &other) {
			if (m_raw_mode_set) {
				tcsetattr(STDIN_FILENO, TCSANOW, &m_orig_term);
				tcflush(STDIN_FILENO, TCIFLUSH);
				std::cin.sync();
				std::cin.clear();
			}
			m_orig_term          = other.m_orig_term;
			m_raw_mode_set       = other.m_raw_mode_set;
			other.m_raw_mode_set = false;
		}
		return *this;
	}

	std::expected<RawTerminalMode, std::string> RawTerminalMode::create() {
		RawTerminalMode guard;

		// Save original terminal settings.
		if (tcgetattr(STDIN_FILENO, &guard.m_orig_term) == -1)
			return std::unexpected<std::string>(base::strConcat(
				"Failed to get terminal attributes: ",
				std::strerror(errno)  // NOLINT(concurrency-mt-unsafe)
			));

		auto new_term = guard.m_orig_term;
		auto flags_mask
			= static_cast<tcflag_t>(static_cast<tcflag_t>(ECHO) | static_cast<tcflag_t>(ICANON));
		new_term.c_lflag &= static_cast<tcflag_t>(~flags_mask);
		new_term.c_cc[VMIN]  = 1;
		new_term.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1)
			return std::unexpected<std::string>(base::strConcat(
				"Failed to set raw terminal mode: ",
				std::strerror(errno)  // NOLINT(concurrency-mt-unsafe)
			));

		guard.m_raw_mode_set = true;
		return guard;
	}

	std::expected<void, std::string> RawTerminalMode::restore() {
		if (!m_raw_mode_set) return {};
		m_raw_mode_set = false;

		int res = tcsetattr(STDIN_FILENO, TCSANOW, &m_orig_term);

		tcflush(STDIN_FILENO, TCIFLUSH);
		std::cin.sync();
		std::cin.clear();

		if (res == -1)
			return std::unexpected<std::string>(base::strConcat(
				"Failed to restore terminal settings: ",
				std::strerror(errno)  // NOLINT(concurrency-mt-unsafe)
			));
		return {};
	}

	IF_BUILD_TYPE_DEV(RawTerminalMode::~RawTerminalMode() {
		auto result = restore();
		CORE_ASSERT_NOEXCEPT(
			result.has_value(), "Failed to restore terminal settings: ", result.error()
		);
	})
	IF_BUILD_TYPE_RELEASE(RawTerminalMode::~RawTerminalMode() noexcept {
		std::ignore = restore();  // best-effort cleanup in Release
	})
}

#elif BASE_TARGET_OS_WINDOWS
	#include <io.h>

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
		if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) return;

		CONSOLE_SCREEN_BUFFER_INFO buffer_info{};
		if (!GetConsoleScreenBufferInfo(hOut, &buffer_info)) {
			writeStr(CLEAR_SCREEN_SEQUENCE);
			return;
		}

		const DWORD cells_count
			= static_cast<DWORD>(buffer_info.dwSize.X) * static_cast<DWORD>(buffer_info.dwSize.Y);
		const COORD home{ 0, 0 };
		DWORD       written = 0;

		if (!FillConsoleOutputCharacterA(hOut, ' ', cells_count, home, &written)) {
			writeStr(CLEAR_SCREEN_SEQUENCE);
			return;
		}

		if (!FillConsoleOutputAttribute(hOut, buffer_info.wAttributes, cells_count, home, &written)) {
			writeStr(CLEAR_SCREEN_SEQUENCE);
			return;
		}

		SetConsoleCursorPosition(hOut, home);
	}

	RawTerminalMode::RawTerminalMode(RawTerminalMode&& other) noexcept:
		  m_orig_in_mode{ other.m_orig_in_mode },
		  m_orig_out_mode{ other.m_orig_out_mode },
		  m_raw_mode_set{ other.m_raw_mode_set } {
		other.m_raw_mode_set = false;
	}

	RawTerminalMode& RawTerminalMode::operator=(RawTerminalMode&& other) noexcept {
		if (this != &other) {
			if (m_raw_mode_set) {
				HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
				HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
				SetConsoleMode(hIn, m_orig_in_mode);
				SetConsoleMode(hOut, m_orig_out_mode);
				FlushConsoleInputBuffer(hIn);
				std::cin.sync();
				std::cin.clear();
			}
			m_orig_in_mode       = other.m_orig_in_mode;
			m_orig_out_mode      = other.m_orig_out_mode;
			m_raw_mode_set       = other.m_raw_mode_set;
			other.m_raw_mode_set = false;
		}
		return *this;
	}

	std::expected<RawTerminalMode, std::string> RawTerminalMode::create() {
		RawTerminalMode guard;

		HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hIn == nullptr || hOut == nullptr || hIn == INVALID_HANDLE_VALUE
		    || hOut == INVALID_HANDLE_VALUE)
			return guard;

		GetConsoleMode(hIn, &guard.m_orig_in_mode);
		GetConsoleMode(hOut, &guard.m_orig_out_mode);

		DWORD new_in_mode = guard.m_orig_in_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
		new_in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

		SetConsoleMode(hIn, new_in_mode);

		DWORD new_out_mode
			= guard.m_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
		SetConsoleMode(hOut, new_out_mode);

		guard.m_raw_mode_set = true;
		return guard;
	}

	std::expected<void, std::string> RawTerminalMode::restore() {
		if (!m_raw_mode_set) return {};
		m_raw_mode_set = false;

		HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		bool   ok   = SetConsoleMode(hIn, m_orig_in_mode);
		ok &= SetConsoleMode(hOut, m_orig_out_mode);

		FlushConsoleInputBuffer(hIn);
		std::cin.sync();
		std::cin.clear();

		if (!ok) return std::unexpected<std::string>("Failed to restore console mode");
		return {};
	}

	IF_BUILD_TYPE_DEV(RawTerminalMode::~RawTerminalMode() {
		auto result = restore();
		CORE_ASSERT_NOEXCEPT(
			result.has_value(), "Failed to restore terminal settings: ", result.error()
		);
	})
	IF_BUILD_TYPE_RELEASE(RawTerminalMode::~RawTerminalMode() noexcept {
		std::ignore = restore();  // best-effort cleanup in Release
	})
}

#else
	#error "Unsupported system"
#endif
