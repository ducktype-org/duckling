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
	class ReplSession {
	public:
		ReplSession();
		explicit ReplSession(ReplConfig config);

		/**
		 * Run the main REPL loop (blocking).
		 * @return Exit code (0 for normal exit)
		 */
		int run();

		ReplResult processLine(const std::string& line);
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

		[[nodiscard]] base::Optional<pst::AccessLocked<pst::ExprStmt>> extractSingleExpression(
			query::Context& ctx, const pst::AccessLocked<pst::LangElement>& root
		) const;

		ReplResult handleExpression(const pst::AccessLocked<pst::ExprStmt>& expr_stmt);
		ReplResult handleDefinition(frontend::ModuleID module_id);

		void initDVM();

		ReplConfig                 m_config;
		std::vector<ReplStatement> m_history;
		bool                       m_should_exit;
		u32                        m_line_counter;
		vm::PID                    m_dvm_pid;
		ReplFrontend               m_frontend;
	};

}  // namespace compiler::repl
