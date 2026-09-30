#pragma once

#include <replxx.hxx>

#include <mutex>

namespace vm::debugger::cli::impl {
	struct ImplementationSpecific {
		replxx::Replxx replxx;              // problem to all the solutions... or smth
		bool           running;             // should we ask user for input once again?
		bool           is_prompt_active;    // so we won't redraw it when it's not
		std::string    current_prompt;      // PROMPT_TEMPLATE filled with status information
		std::string    current_line;        // needed to redraw with prompt
		std::mutex     current_line_mutex;  // trying to make it thread safe
	};
}

// override so linter knows whats going on
#define USE_REPLXX  // NOLINT(clang-diagnostic-macro-redefined)
#include <vm/debugger/UI/CLI/cli.hpp>
