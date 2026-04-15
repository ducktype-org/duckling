/**
 * @file session.hpp
 * @brief REPL (Read-Eval-Print Loop) session management for Duckling compiler.
 *
 * This file defines the ReplSession class which manages an interactive REPL session,
 * including handling user input, managing history, and executing code.
 */

#pragma once

#include "frontend.hpp"
#include "helper_structs.hpp"

#include <backends/dvm/repl_lowering.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/core/process/interface_types.hpp>

#include <string_view>
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

		/**
		 * Run the main REPL loop (blocking).
		 * @return Exit code (0 for normal exit)
		 */
		int run();

	private:
		/**
		 * @brief Grant ReplSimulationTest access to private members for testing.
		 *
		 * Allows the test suite to access private methods and members to verify
		 * internal behavior without exposing them in the public API.
		 */
		friend class ReplSimulationTest;
		/**
		 * @brief Process a single line of input from the user.
		 *
		 * Determines whether the line is a command (starts with '/') or code to execute,
		 * and routes it to the appropriate handler.
		 *
		 * @param line The input line from the user
		 * @return ReplResult indicating success, error, or exit status
		 */
		ReplResult processLine(std::string_view line);

		/**
		 * @brief Execute user-provided code, splitting it into individual statements.
		 *
		 * Parses the full input to validate syntax and extract statement boundaries,
		 * then creates a separate module for each top-level statement and executes
		 * them in order. Stops at the first error.
		 *
		 * @param input The code to execute (may contain multiple statements)
		 * @return ReplResult with execution outcome and optional message
		 */
		ReplResult executeInput(std::string_view input);

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

		/**
		 * @brief Clear the REPL session history.
		 *
		 * Removes all previously entered statements from the session history
		 * and resets the line counter.
		 */
		void clearHistory();


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
		[[nodiscard]] bool isCommand(std::string_view line) const;

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
		bool handleCommand(std::string_view line);

		/**
		 * @brief Execute a single statement module.
		 *
		 * Determines whether the module contains a lone expression (to be evaluated
		 * and printed) or a definition (to be loaded into the environment) and routes
		 * accordingly.
		 *
		 * @note This function works only on single statements. Splitting the input into statements
		 * is the responsibility of executeInput().
		 *
		 * @param module_id Already-created module for this statement
		 * @return ReplResult with execution outcome
		 */
		ReplResult executeSingleStatement(frontend::ModuleID module_id);

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
		 * @param stmt The top-level statement being treated as a definition
		 * @return ReplResult indicating success or error
		 */
		ReplResult handleDefinition(const pst::AccessLocked<pst::Stmt>& stmt);

		/**
		 * @brief Compile and execute a single instruction (if/while/for/block) in the REPL.
		 *
		 * Wraps the instruction in a synthetic void function, compiles it to DVM bytecode,
		 * executes it, and reports the outcome.
		 *
		 * @param stmt The instruction statement to execute
		 * @return ReplResult indicating success or error
		 */
		ReplResult handleInstruction(const pst::AccessLocked<pst::Stmt>& stmt);

		/**
		 * @brief Return the currently processed REPL module.
		 *
		 * The current module is the most recently created module in REPL history.
		 * It is later compiled to HOUT and sent to DVM for execution.
		 */
		[[nodiscard]] frontend::ModuleID getCurrentModuleID() const;

		/**
		 * @brief Initialize the DVM process for code execution.
		 *
		 * Spawns a new DVM process and attaches I/O streams. Should be called
		 * once during REPL session initialization.
		 */
		void initDVM();

		bool                       m_should_exit;  ///< Flag to terminate the REPL loop
		std::vector<ReplStatement> m_history;      ///< All statements entered in this session
		u64          m_line_counter;  ///< Counter for generating unique wrapper function names
		vm::PID      m_dvm_pid;       ///< Process ID of the running DVM instance
		ReplFrontend m_frontend;      ///< Frontend for user interaction
		/**
		 * @brief Persistent lowering context for REPL statement compilation.
		 *
		 * Maintains state across multiple REPL statements, allowing accumulated functions,
		 * globals, and types from previous statements to be referenced in new statements.
		 * Similar to DVMBuilder but with incremental loading semantics for interactive sessions.
		 */
		base::Optional<backend_vm::ReplLoweringContext> m_lowering_context;
	};

}  // namespace compiler::repl
