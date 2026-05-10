#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/pointers/ref.hpp>

#include <ostream>
#include <string>
#include <utility>

namespace compiler::repl {
	/**
	 * @brief Configuration options for REPL behavior and appearance.
	 */
	struct ReplConfig final {
		static constexpr std::string PROMPT
			= "duckling> ";  /// Primary prompt shown before each input
		static constexpr std::string CONTINUATION = "          ";  /// Prompt for continuation lines
		static constexpr std::string HISTORY_MULTILINE_CONTINUATION
			= "    ";  /// Prompt for history continuation.
	};

	/**
	 * @brief Represents a single statement entered in the REPL session.
	 *
	 * Each statement maintains the original source code and associated module tree,
	 * allowing the REPL to track and reference previously executed code.
	 */
	struct ReplStatement final {
		std::string                      source_code;
		base::CRef<frontend::ModuleTree> module;
		frontend::ModuleID               module_id;

		ReplStatement(std::string code, frontend::ModuleID mod_id):
			  source_code(std::move(code)),
			  module(frontend::getModuleRef(mod_id)),
			  module_id(mod_id) {}
	};

	/**
	 * @brief Result of processing a REPL input
	 *
	 * Contains the status of the operation and an optional message.
	 */
	struct ReplResult final {
		enum class Status { Success, Error, Exit, Reset, IncompleteInput };

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

		static ReplResult reset(std::string msg = "Restarting REPL...") {
			return ReplResult{ .status = Status::Reset, .message = std::move(msg) };
		}

		static ReplResult incomplete() {
			return ReplResult{ .status = Status::IncompleteInput, .message = "" };
		}
	};

	/**
	 * @brief Print shared REPL commands help section.
	 *
	 * This section is frontend-agnostic and should remain identical between
	 * minimal and replxx implementations.
	 */
	inline void printReplCommandsHelp(std::ostream& out) {
		out << "\n=== REPL Commands ===\n";
		out << "  /help, /?, /h                   - Show this help message\n";
		out << "  /exit, /quit, /q                - Exit the REPL\n";
		out << "  /reset [n|-n]                   - Restart the REPL process to the state number "
			   "n|to the state n entries ago(for this option, need to provide - before n). Without "
			   "providing any number, it will reset to the clean starting state.";
		out << "  /history, /hist                 - Show session history (all statements executed "
			   "in this repl session)\n";
		out << "  /commands, /cmds                - Show all input history (editor history)\n";
		out << "  /commands-reset, /cmds-reset    - Clear input history (editor history)\n";
		out << "  /clear, /c                      - Clear terminal\n";
		out << "  /load <file.ds>                 - Load script file (stops on first error; "
		       "previous statements stay applied)\n";
	}
}  // namespace compiler::repl
