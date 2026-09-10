#pragma once

#ifndef _WIN32
	#include <termios.h>
#else
	#include <windows.h>
#endif

#include <expected>
#include <string>
#include <string_view>

namespace os_utils {

	/// @brief Writes a string to the standard output.
	void writeStr(std::string_view str);

	/// @brief Writes a single character to the standard output.
	void writeChar(char c);

	/// @brief Reads a single character from the standard input.
	/// @return True if a character was read, false on EOF or error.
	bool readChar(char& c);

	/// @brief Clears the terminal screen.
	void clearScreen();

	/**
	 * @brief RAII guard that switches the terminal to raw mode and restores it on destruction.
	 *
	 * In raw mode, input is read character-by-character without line buffering or echo.
	 * The original terminal settings are restored when the guard goes out of scope.
	 *
	 * Create one via RawTerminalMode::create(). The type is move-only.
	 */
	class RawTerminalMode final {
	public:
		/**
		 * @brief Switches the terminal to raw mode.
		 * @return A guard that restores the previous settings on destruction, or an
		 *         error message.
		 */
		static std::expected<RawTerminalMode, std::string> create();

		/**
		 * @brief Restores the original terminal settings.
		 *
		 * The explicit, checkable alternative to the best-effort destructor
		 * cleanup. Safe to call multiple times; subsequent calls are no-ops.
		 * @return An error message if the restore failed.
		 */
		std::expected<void, std::string> restore();

		RawTerminalMode(RawTerminalMode&& other) noexcept;
		RawTerminalMode& operator=(RawTerminalMode&& other) noexcept;
		RawTerminalMode(const RawTerminalMode&)            = delete;
		RawTerminalMode& operator=(const RawTerminalMode&) = delete;
		~RawTerminalMode();

	private:
		RawTerminalMode() = default;

#ifndef _WIN32
		termios m_orig_term{};
#else
		DWORD m_orig_in_mode{};
		DWORD m_orig_out_mode{};
#endif
		bool m_raw_mode_set = false;
	};

}
