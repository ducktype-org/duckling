#pragma once

#include "../helper_structs.hpp"

#include <replxx.hxx>

#include <set>
#include <string>

namespace compiler::repl {
	class FrontendReplxxImplementation final {
	public:
		explicit FrontendReplxxImplementation(bool completions_enabled = true);
		~FrontendReplxxImplementation();

		void        printWelcome() const;
		std::string readLine();
		void        printHistory() const;
		void        clearHistory();
		void        clearScreen();
		void        printHelp() const;

	private:
		/// Setup replxx key bindings, callbacks, and multiline behavior.
		void setupKeyBindings();
		void setupHighlighter();
		void setupCompletion();
		void setupHints();

		/// Collect identifiers from a line of input into the user word set.
		void collectIdentifiers(const std::string& input);

		/// Get the path to the persistent history file (~/.duckling_repl_history).
		static std::string getHistoryFilePath();

		replxx::Replxx m_replxx;
		bool           m_completions_enabled;

		/// Set of user-defined identifiers collected from previous inputs (for completion).
		std::set<std::string> m_user_words;
	};
}
