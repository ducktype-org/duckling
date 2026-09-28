#pragma once

#include <base/types/ints.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace compiler::repl {
	class FrontendMinImplementation final {
	public:
		explicit FrontendMinImplementation(
			bool completions_enabled     = false,
			bool bracketed_paste_enabled = false,
			bool decorative_output       = true
		);
		~FrontendMinImplementation() = default;

		void        printWelcome() const;
		std::string readLine();
		void        printHistory() const;
		void        addHistoryEntry(std::string_view entry);
		void        clearHistory();
		void        clearScreen();
		void        printHelp() const;
		void        printCompletions(std::string_view prefix) const;

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
		bool                     m_bracketed_paste_enabled;
		bool                     m_decorative_output;
		bool                     m_in_bracketed_paste = false;
	};
}
