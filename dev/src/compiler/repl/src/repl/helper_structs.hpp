#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/pointers/ref.hpp>

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
}  // namespace compiler::repl
