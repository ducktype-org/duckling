#pragma once

#ifndef _WIN32
	#include <termios.h>
#else
	#include <windows.h>
#endif

#include <string_view>

namespace os_utils {

	void writeStr(std::string_view str);
	void writeChar(char c);
	bool readChar(char& c);
	void clearScreen();

	/**
	 * @brief RAII guard that switches the terminal to raw mode and restores it on destruction.
	 *
	 * In raw mode, input is read character-by-character without line buffering or echo.
	 * The original terminal settings are restored when the guard goes out of scope.
	 */
	class RawTerminalMode {
	public:
		RawTerminalMode();
		~RawTerminalMode();

	private:
#ifndef _WIN32
		struct termios m_orig_term{};
#else
		DWORD m_orig_in_mode{};
		DWORD m_orig_out_mode{};
#endif
		bool m_raw_mode_set = false;
	};

}
