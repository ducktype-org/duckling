/**
 * @file repl_session.hpp
 * @brief REPL (Read-Eval-Print Loop) session management for Duckling compiler.
 *
 * This file defines the ReplSession class which manages an interactive REPL session,
 * including handling user input, managing history, and executing code.
 */

#pragma once

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

	struct ReplStatement {
		std::string                     source_code;
		base::Ref<frontend::ModuleTree> module;
		frontend::ModuleID              module_id;

		ReplStatement(std::string code, base::Ref<frontend::ModuleTree> mod):
			  source_code(std::move(code)),
			  module(mod),
			  module_id(module->getModuleID()) {}
	};

	struct ReplConfig {
		std::string prompt          = "duckling> ";
		std::string continuation    = "      |";
		std::string multiline_start = R"(""")";
		std::string multiline_end   = "/end";
		bool        show_hout_debug = true;
		bool        run_dvm         = true;
	};

	/**
	 * @brief Result of processing a REPL input
	 *
	 * Contains the status of the operation and an optional message.
	 */
	struct ReplResult {
		enum class Status { Success, Error, Exit, IncompleteInput };

		Status      status;
		std::string message;

		static ReplResult success(std::string msg = "") {
			return ReplResult{ .status = Status::Success, .message = std::move(msg) };
		}

		static ReplResult error(std::string msg) {
			return ReplResult{ .status = Status::Error, .message = std::move(msg) };
		}

		static ReplResult exit() {
			return ReplResult{ .status = Status::Exit, .message = "Goodbye!" };
		}

		static ReplResult incomplete() {
			return ReplResult{ .status = Status::IncompleteInput, .message = "" };
		}
	};

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

		void printWelcome() const;

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

		void printHistory() const;
		void printHelp() const;
		void clearHistory();

	private:
		std::string        handleMultilineInput();
		[[nodiscard]] bool isCommand(const std::string& line) const;
		bool               handleCommand(const std::string& line);
		void               printPrompt() const;

		[[nodiscard]] base::Optional<pst::AccessLocked<pst::ExprStmt>> extractSingleExpression(
			query::Context& ctx, const pst::AccessLocked<pst::LangElement>& root
		) const;

		ReplResult handleExpression(const pst::AccessLocked<pst::ExprStmt>& expr_stmt);
		ReplResult handleDefinition(frontend::ModuleID module_id);

		void                       initDVM();
		ReplConfig                 m_config;
		std::vector<ReplStatement> m_history;
		bool                       m_should_exit;
		u32                        m_line_counter;

		vm::PID m_dvm_pid;
	};

}  // namespace compiler::repl
