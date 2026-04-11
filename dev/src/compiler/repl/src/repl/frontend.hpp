/**
 * @file frontend.hpp
 * @brief Frontend interface for REPL session.
 */

#pragma once

#include <string>

#ifdef USE_REPLXX
	#include "frontend_implementations/replxx.hpp"
using FrontendImplementationType = compiler::repl::FrontendReplxxImplementation;
#else
	#include "frontend_implementations/minimal.hpp"
using FrontendImplementationType = compiler::repl::FrontendMinImplementation;
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

		/**
		 * @brief Prints the welcome message when starting the REPL.
		 */
		void printWelcome() const;

		/**
		 * @brief Blocks and waits for the user to enter next piece of input.
		 *
		 * @return The string of input provided by the user.
		 */
		std::string readLine();

		/**
		 * @brief Prints the history of previously entered inputs.
		 */
		void printHistory() const;

		/**
		 * @brief Clears the terminal screen.
		 */
		void clearScreen();

		/**
		 * @brief Clears the history of previously entered inputs.
		 */
		void clearHistory();

		/**
		 * @brief Prints the help message with available commands and controls.
		 */
		void printHelp() const;

	private:
		FrontendImplementationType m_impl;
	};  // class ReplFrontend

}  // namespace compiler::repl
