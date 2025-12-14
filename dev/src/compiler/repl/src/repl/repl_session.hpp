/**
 * @file repl_session.hpp
 * @brief REPL (Read-Eval-Print Loop) session management for Duckling compiler.
 *
 * This file defines the ReplSession class which manages an interactive REPL session,
 * including handling user input, managing history, and executing code.
 */

#pragma once

#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

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

	struct ReplResult {
		enum class Status { Success, Error, Exit, IncompleteInput };

		Status      status;
		std::string message;
		i32         exit_code = 0;

		static ReplResult success(std::string msg = "", i32 code = 0) {
			return ReplResult{ .status    = Status::Success,
				               .message   = std::move(msg),
				               .exit_code = code };
		}

		static ReplResult error(std::string msg) {
			return ReplResult{ .status = Status::Error, .message = std::move(msg), .exit_code = 0 };
		}

		static ReplResult exit() {
			return ReplResult{ .status = Status::Exit, .message = "Goodbye!", .exit_code = 0 };
		}

		static ReplResult incomplete() {
			return ReplResult{ .status = Status::IncompleteInput, .message = "", .exit_code = 0 };
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

		void clearAccumulated() { m_accumulated_input.clear(); }

		[[nodiscard]]
		bool shouldExit() const {
			return m_should_exit;
		}

		void printHistory() const;
		void printHelp() const;
		void clearHistory();

	private:
		std::string                handleMultilineInput();
		[[nodiscard]] bool         isCommand(const std::string& line) const;
		bool                       handleCommand(const std::string& line);
		void                       printPrompt() const;
		ReplConfig                 m_config;
		std::vector<ReplStatement> m_history;
		std::string                m_accumulated_input;
		bool                       m_should_exit;
		u32                        m_line_counter;
	};

}  // namespace compiler::repl
