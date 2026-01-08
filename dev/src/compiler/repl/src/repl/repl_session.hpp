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

		[[nodiscard]]
		const std::vector<ReplStatement>& getHistory() const {
			return m_history;
		}

		[[nodiscard]]
		const ReplConfig& getConfig() const {
			return m_config;
		}

		[[nodiscard]]
		bool shouldExit() const {
			return m_should_exit;
		}

		void clearHistory();

	private:
		[[nodiscard]] bool isCommand(const std::string& line) const;
		bool               handleCommand(const std::string& line);

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
