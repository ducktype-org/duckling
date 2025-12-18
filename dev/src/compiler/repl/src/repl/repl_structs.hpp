#pragma once

#include <string>
#include <utility>

#include "base/pointers/ref.hpp"
#include "frontend/module_tree/module_tree.hpp"
#include "frontend/module_tree/file_id.hpp"

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
} // namespace compiler::repl
