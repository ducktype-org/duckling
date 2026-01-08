#pragma once

#include "frontend/module_tree/file_id.hpp"
#include "frontend/module_tree/functors.hpp"
#include "frontend/module_tree/module_tree.hpp"

#include "base/pointers/ref.hpp"

#include <string>
#include <utility>

namespace compiler::repl {
	struct ReplStatement {
		std::string                      source_code;
		base::CRef<frontend::ModuleTree> module;
		frontend::ModuleID               module_id;

		ReplStatement(std::string code, frontend::ModuleID mod_id):
			  source_code(std::move(code)),
			  module(frontend::getModuleRef(mod_id)),
			  module_id(mod_id) {}
	};

	struct ReplConfig {
		std::string prompt          = "duckling> ";
		std::string continuation    = "      |";
		std::string multiline_start = R"(""")";
		std::string multiline_end   = "/end";
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
}  // namespace compiler::repl
