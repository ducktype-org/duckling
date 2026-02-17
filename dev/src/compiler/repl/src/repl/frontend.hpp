/**
 * @file frontend.hpp
 * @brief Frontend interface for REPL session.
 */

#pragma once

#ifdef USE_REPLXX
	#include "frontend_implementations/replxx.hpp"
#else
	#include "frontend_implementations/minimal.hpp"
#endif

#include <string>


#ifdef USE_REPLXX
using impl_t = compiler::repl::FrontendReplxxImplementation;
#else
using impl_t = compiler::repl::FrontendMinImplementation;
#endif

namespace compiler::repl {

	/**
	 * @brief Frontend interface for REPL session.
	 *
	 * Handles interactions in console using the replxx line-editing library.
	 * Provides prompts, reads user input with history navigation, inline editing,
	 * and single- and multi-line input modes.
	 *
	 * Features:
	 *  - Multiline editing: press Enter to create a new line.
	 *  - Commit input: press Alt+Enter (or Ctrl+J) to submit the current input.
	 *  - Tab completion: press Tab to complete based on keywords and previously typed identifiers.
	 *  - Syntax coloring: keywords, types, literals, and operators are highlighted.
	 *  - History: previously committed inputs are persisted across sessions.
	 *
	 * Example usage:
	 *  after entering repl with:
	 *  	duckc repl
	 *  type /help to see available commands.
	 */
	class ReplFrontend final {
	public:
		ReplFrontend();
		~ReplFrontend() = default;

		void        printWelcome() const;
		std::string readLine();
		void        printHistory() const;
		void        clearHistory();
		void        printHelp() const;

	private:
		impl_t m_impl;
	};  // class ReplFrontend

}  // namespace compiler::repl
