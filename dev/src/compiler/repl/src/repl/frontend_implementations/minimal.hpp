#pragma once

#ifndef _WIN32
	#include <termios.h>
#else
	#include <windows.h>
#endif

#include <base/types/ints.hpp>

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
#ifndef _WIN32
		struct termios m_orig_term{};
#else
		DWORD m_orig_in_mode{};
		DWORD m_orig_out_mode{};
#endif
		bool m_raw_mode_set = false;
	};
}

namespace compiler::repl {
	class FrontendMinImplementation final {
	public:
		FrontendMinImplementation();
		~FrontendMinImplementation() = default;

		void        printWelcome() const;
		std::string readLine();
		void        printHistory() const;
		void        clearHistory();
		void        printHelp() const;

	private:
		struct EditorState {
			u64                      row{}, col{};
			std::vector<std::string> lines;
			std::vector<u64>         prev_state_lengths = { 0 };
			u64                      prev_state_row{}, prev_state_col{};

			EditorState()                         = default;
			EditorState(const EditorState& other) = default;
			EditorState& operator=(const EditorState& other);

			void                      reset();
			[[nodiscard]] std::string print(bool for_history = false) const;
			[[nodiscard]] std::string contentFromRow(const u64 from_row) const;
			[[nodiscard]] std::string rawContent() const;
		};

		void moveCursorLeft() const;
		void moveCursorRight() const;
		void moveCursorUp() const;
		void moveCursorFromEndToPos(u64 pos, std::string& line) const;
		void moveCursorToNewLine() const;

		void onEscapeSequence();
		void onPrintableChar(char c);

		void onArrowLeft();
		void onArrowRight();
		void onArrowUp();
		void onArrowDown();
		void historyScrollUp();
		void historyScrollDown();

		void saveToHistory();

		void clearScreen();

		void refreshLinesFromCursorAndBelow();

		std::string sequenceToMoveCursorFromGivenPosToStart(u64 row, u64 col);
		std::string sequenceToMoveCursorFromEndToCurrentPosition();
		std::string clearScreenSequence();
		void        refreshScreen();
		void        updatePrevState();

		std::string internalReadLine();
		void        onNewLine();
		void        onBackspace();

		std::vector<EditorState> m_history;
		u64                      m_hist_idx;
		EditorState              m_editor_state;
		EditorState              m_stashed_editor_state;
		const std::string        m_sequence_to_align_cursor_to_multiline_start;
	};
}
