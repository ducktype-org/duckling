#include "cli.hpp"

#include <diagnostic/highlight_positions.hpp>
#include <fstream>


namespace {
	// the path to the history file
	static const std::string history_file_path{ "./DVM_debugger_replxx_history.txt" };
}

namespace vm::debugger::cli {
	void CLIDebugger::specInit() {
		spec.replxx.install_window_change_handler();

		/* scope for ifstream object for auto-close */ {
			std::ifstream history_file(history_file_path);
			spec.replxx.history_load(history_file);
		}

		spec.replxx.set_max_history_size(128);
		spec.replxx.set_max_hint_rows(3);
		spec.replxx.enable_bracketed_paste();

		spec.replxx.bind_key_internal(Replxx::KEY::BACKSPACE, "delete_character_left_of_cursor");
		spec.replxx.bind_key_internal(Replxx::KEY::DELETE, "delete_character_under_cursor");
		spec.replxx.bind_key_internal(Replxx::KEY::LEFT, "move_cursor_left");
		spec.replxx.bind_key_internal(Replxx::KEY::RIGHT, "move_cursor_right");
		// spec.replxx.bind_key_internal( Replxx::KEY::UP, "line_previous" );
		// spec.replxx.bind_key_internal( Replxx::KEY::DOWN, "line_next" );
		spec.replxx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::UP), "history_previous");
		spec.replxx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::DOWN), "history_next");
		spec.replxx.bind_key_internal(Replxx::KEY::PAGE_UP, "history_first");
		spec.replxx.bind_key_internal(Replxx::KEY::PAGE_DOWN, "history_last");
		spec.replxx.bind_key_internal(Replxx::KEY::HOME, "move_cursor_to_begining_of_line");
		spec.replxx.bind_key_internal(Replxx::KEY::END, "move_cursor_to_end_of_line");
	}

	void CLIDebugger::specExit() {
		// spec.replxx.history_sync(history_file_path);
		spec.replxx.disable_bracketed_paste();
	}

	bool CLIDebugger::getline(std::string& line) {
		const char* cinput{ nullptr };
		
		std::string prompt = "\x1b[1;32mBeRD\x1b[0m> ";

		do { cinput = spec.replxx.input(prompt); } while ((cinput == nullptr) && (errno == EAGAIN));
		if (cinput == nullptr) return false;

		line = cinput;

		spec.replxx.history_add(line);
		return true;
	}

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		std::stringstream stream;
		printer::StreamPrinter::print(content, stream);
		spec.replxx.print(stream.str().c_str());
	}

	void CLIDebugger::printNL(const printer::PrinterContentsSeq& content) {
		std::stringstream stream;
		printer::StreamPrinter::print(content, stream);
		printer::StreamPrinter::newline(1, stream);
		spec.replxx.print(stream.str().c_str());
	}

	void CLIDebugger::printError(const printer::PrinterContentsSeq& content) {
		std::stringstream stream;
		printer::StreamPrinter::print(
			{ { "[Debug error]: ", printer::Color::BrightRed } }, stream
		);
		printer::StreamPrinter::print(content, stream);
		printer::StreamPrinter::newline(1, stream);
		spec.replxx.print(stream.str().c_str());
	}
}
