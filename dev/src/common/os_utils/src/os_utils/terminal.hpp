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
	 * @brief RawTerminalMode
	 *
	 * Changes the operating mode of the terminal. Uses RAII to ensure terminal settings are
	 * untouched upon exiting repl.
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
