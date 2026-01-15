/**
 * @file repl_session.hpp
 * @brief REPL (Read-Eval-Print Loop) session management for Duckling compiler.
 *
 * This file defines the ReplSession class which manages an interactive REPL session,
 * including handling user input, managing history, and executing code.
 */

#pragma once

#include "repl_frontend.hpp"
#include "repl_structs.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/interface_types.hpp>

#include <string>
#include <vector>

namespace compiler::repl {
	/**
	 * @brief Manages an interactive REPL session for Duckling compiler.
	 *
	 * Maintains history of all submitted statements, configuration, and handles
	 * input accumulation for multiline statements.
	 */
	class ReplSession final {
	public:
		ReplSession();
		explicit ReplSession(ReplConfig config);

		/**
		 * Run the main REPL loop (blocking).
		 * @return Exit code (0 for normal exit)
		 */
		int run();

		/**
		 * @brief Process a single line of input from the user.
		 *
		 * Determines whether the line is a command (starts with '/') or code to execute,
		 * and routes it to the appropriate handler.
		 *
		 * @param line The input line from the user
		 * @return ReplResult indicating success, error, or exit status
		 */
		ReplResult processLine(const std::string& line);

		/**
		 * @brief Execute user-provided code as either an expression or definition.
		 *
		 * Creates a module from the input, parses it as PST, and determines whether
		 * it's a single expression (to be evaluated) or a definition (to be loaded).
		 *
		 * @param input The code to execute
		 * @return ReplResult with execution outcome and optional message
		 */
		ReplResult executeInput(const std::string& input);

		/**
		 * @brief Get the history of all statements entered in this REPL session.
		 *
		 * @return Vector of ReplStatement representing the session history.
		 */
		[[nodiscard]]
		const std::vector<ReplStatement>& getHistory() const {
			return m_history;
		}

		/**
		 * @brief Get the configuration settings for this REPL session.
		 *
		 * @return ReplConfig containing the session's configuration options.
		 */
		[[nodiscard]]
		const ReplConfig& getConfig() const {
			return m_config;
		}

		/**
		 * @brief Check whether the REPL session should terminate.
		 *
		 * Returns the exit flag that indicates whether the user has requested to exit
		 * the REPL session. This flag is set to true when the user enters one of the
		 * exit commands (/exit, /quit, or /q) or when an end-of-file (EOF) is detected
		 * on standard input. The main REPL loop checks this flag to determine when to
		 * stop prompting for input and gracefully terminate the session.
		 *
		 * @return true if the REPL should exit, false if it should continue running
		 */
		[[nodiscard]]
		bool shouldExit() const {
			return m_should_exit;
		}

		void clearHistory();

	private:
		/**
		 * @brief Determine if a line of input is a REPL command.
		 *
		 * Checks whether the given input line represents a command rather than code to
		 * execute. Commands in the REPL are distinguished by starting with a forward slash '/'
		 * character, such as /exit, /help, /history, etc. This allows users to control the
		 * REPL session and access utilities without conflicting with valid Duckling code
		 * syntax.
		 *
		 * @param line The input line to check
		 * @return true if the line is a command (starts with '/'), false otherwise
		 */
		[[nodiscard]] bool isCommand(const std::string& line) const;

		/**
		 * @brief Process and execute a REPL command.
		 *
		 * Parses and executes commands that control the REPL session behavior.

		 * If an unrecognized command is provided, displays an error message and suggests using
		 * /help. This method is called by processLine() when the input is identified as a command.
		 *
		 * @param line The command line to process (must start with '/')
		 * @return true if the command was recognized and handled, false if unrecognized
		 */
		bool handleCommand(const std::string& line);

		/**
		 * @brief Extracts a single expression statement from the PST root, if present.
		 *
		 * This determines whether the user input is a standalone expression that should
		 * be evaluated and printed, versus a definition (function, variable, etc.) that
		 * should be loaded into the environment.
		 *
		 * @param ctx Query context for PST access
		 * @param root The PST root element to examine
		 * @return Optional containing the ExprStmt if input is a single expression, empty otherwise
		 */
		[[nodiscard]] base::Optional<pst::AccessLocked<pst::ExprStmt>> extractSingleExpression(
			query::Context& ctx, const pst::AccessLocked<pst::LangElement>& root
		) const;

		/**
		 * @brief Compile and execute a single expression, then print its result.
		 *
		 * Wraps the expression in a function, compiles it to DVM bytecode, executes it,
		 * and formats the return value for display.
		 *
		 * @param expr_stmt The expression statement to evaluate
		 * @return ReplResult with formatted expression value or error
		 */
		ReplResult handleExpression(const pst::AccessLocked<pst::ExprStmt>& expr_stmt);

		/**
		 * @brief Compile and load definitions (functions, variables, etc.) into the REPL environment.
		 *
		 * @param module_id The module containing the definitions to load
		 * @return ReplResult indicating success or error
		 */
		ReplResult handleDefinition(frontend::ModuleID module_id);

		/**
		 * @brief Initialize the DVM process for code execution.
		 *
		 * Spawns a new DVM process and attaches I/O streams. Should be called
		 * once during REPL session initialization.
		 */
		void initDVM();

		ReplConfig                 m_config;       /// Configuration for REPL behavior
		std::vector<ReplStatement> m_history;      /// All statements entered in this session
		bool                       m_should_exit;  /// Flag to terminate the REPL loop
		u64          m_line_counter;  /// Counter for generating unique wrapper function names
		vm::PID      m_dvm_pid;       /// Process ID of the running DVM instance
		ReplFrontend m_frontend;      /// Frontend for user interaction
	};

}  // namespace compiler::repl
