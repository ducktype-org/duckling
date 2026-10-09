// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "cli.hpp"
#include "common.hpp"

#include <replxx.hxx>

#include <fstream>
#include <mutex>

#define PROMPT_TEMPLATE   "\x1b[1;32mBeRD\x1b[0m {} \x1b[1m>>>\x1b[0m "
#define HISTORY_FILE_NAME ".duckling_debugger_history"

// it's not a theft, it's a piracy ;)
namespace replxx::tty {
	extern bool in;
	extern bool out;
}

namespace {
	/**
	 * @brief Path to the history file
	 */
	inline std::filesystem::path getHistoryFilePath() {
		const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe)
		if (home != nullptr) return std::filesystem::path(home) / HISTORY_FILE_NAME;
		return HISTORY_FILE_NAME;
	}

	inline bool isInteractive() { return replxx::tty::in && replxx::tty::out; }

	/**
	 * @brief mc - maybe colors
	 */
	inline std::string mc(const std::string& original) {
		if (isInteractive()) return original;
		return vm::debugger::cli::common::withoutControlSequences(original);
	}
}

namespace vm::debugger::cli {
	using Replxx = replxx::Replxx;

	namespace impl {
		struct ImplementationSpecific {
			replxx::Replxx    replxx;                    // problem to all the solutions... or smth
			bool              running          = false;  // should we ask user for input once again?
			std::atomic<bool> is_prompt_active = false;  // so we won't redraw it when it's not
			std::string       current_prompt;      // PROMPT_TEMPLATE filled with status information
			std::string       current_line;        // needed to redraw with prompt
			std::mutex        current_line_mutex;  // trying to make it thread safe
		};
	}

	CLIDebugger::CLIDebugger(): spec(new impl::ImplementationSpecific()) {}

	CLIDebugger::~CLIDebugger() = default;

	void CLIDebugger::mainLoop(clah::Clah& clah) {
		spec->replxx.install_window_change_handler();

		if (isInteractive()) {
			std::ifstream history_file(getHistoryFilePath());
			spec->replxx.history_load(history_file);
		}

		spec->replxx.set_max_history_size(128);
		spec->replxx.set_max_hint_rows(3);
		if (isInteractive()) spec->replxx.enable_bracketed_paste();

		printNL(mc(
			"\x1b[1mWelcome to \x1b[32mBeRD\x1b[0;1m - an interactive in-DVM debugger!\x1b[0m (now "
			"with replxx support!)"
		));

		spec->running = true;
		spec->current_prompt
			= mc(std::format(PROMPT_TEMPLATE, vm::api::statusName(debugger.getStatus())));

		events::Listener<api::ProcStatus> status_change_listener([&](const api::ProcStatus& status) {
			std::stringstream sstr;

			sstr << vm::api::statusName(status);
			for (std::string& value: common::extractPrimitiveValues(status))
				sstr << "(" << value << ")";

			if (!isInteractive()) printNL(common::statusLine(status));

			std::lock_guard _(spec->current_line_mutex);
			spec->current_prompt = mc(std::format(PROMPT_TEMPLATE, sstr.str()));
			spec->replxx.set_prompt(spec->current_line + spec->current_prompt);
		});

		debugger.attachOnStatusChangedListener(status_change_listener);

		auto getline = [&](std::string& line) {
			const char* cinput{ nullptr };

			spec->is_prompt_active = true;

			do {
				std::string prompt;
				{
					std::lock_guard _(spec->current_line_mutex);
					prompt = spec->current_line + spec->current_prompt;
				}
				cinput = spec->replxx.input(prompt);
				std::lock_guard _(spec->current_line_mutex);
				spec->current_line = "";
			} while ((cinput == nullptr) && (errno == EAGAIN));

			spec->is_prompt_active = false;
			if (cinput == nullptr) return false;

			line = cinput;

			if (isInteractive()) spec->replxx.history_add(line);
			return true;
		};

		for (std::string line; spec->running && getline(line); clah.execute(common::strip(line)));

		if (spec->running && isInteractive()) printNL("exit");
		spec->running = false;

		status_change_listener.detach();

		printNL("Exiting debugger.");

		if (isInteractive()) spec->replxx.history_sync(getHistoryFilePath());
		spec->replxx.disable_bracketed_paste();
	}

	void CLIDebugger::exitMainLoop() { spec->running = false; }

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		// a lot of copying for now, to prevent replxx from eating every thing printed without newline

		std::stringstream sstr;
		printer::StreamPrinter::print(content, sstr);
		std::string content_string = sstr.str();

		std::lock_guard _(spec->current_line_mutex);

		std::string before_content = spec->current_line;

		spec->current_line = spec->current_line + content_string;
		if (std::size_t pos = spec->current_line.find_last_of('\n'); pos != std::string::npos)
			spec->current_line = spec->current_line.substr(pos + 1);

		if (spec->is_prompt_active && isInteractive()) {
			spec->replxx.write(before_content.c_str(), static_cast<int>(before_content.length()));
			spec->replxx.write(content_string.c_str(), static_cast<int>(content_string.length()));
			spec->replxx.set_prompt(spec->current_line + spec->current_prompt);
		} else {
			spec->replxx.write(content_string.c_str(), static_cast<int>(content_string.length()));
		}
	}
}
