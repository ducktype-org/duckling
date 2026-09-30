#include "cli.hpp"

#include <fstream>

#define PROMPT_TEMPLATE     "\x1b[1;32mBeRD\x1b[0m {} \x1b[1m>>>\x1b[0m "
#define END_PROMPT_TEMPLATE "\x1b[1;32mBeRD\x1b[0m {}"

namespace {
	std::string strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	template<typename T>
	std::string typeToString(const T& status) {
		return std::visit(
			[&](auto&& arg) {
				using TT = std::decay_t<decltype(arg)>;
				return TypeParseTraits<TT>::NAME.data();
			},
			status
		);
	}

	/**
	 * @brief Path to the history file
	 *
	 * It make sense to keep it in cwd so it's created per project.
	 */
	static const std::string HISTORY_FILE_PATH{ "./duckling_debugger_history" };
}

namespace vm::debugger::cli {
	using Replxx = replxx::Replxx;

	void CLIDebugger::mainLoop(clah::Clah& clah) {
		spec.replxx.install_window_change_handler();

		/* scope for ifstream object for auto-close */ {
			std::ifstream history_file(HISTORY_FILE_PATH);
			spec.replxx.history_load(history_file);
		}

		spec.replxx.set_max_history_size(128);
		spec.replxx.set_max_hint_rows(3);
		spec.replxx.enable_bracketed_paste();

		spec.replxx.bind_key_internal(Replxx::KEY::BACKSPACE, "delete_character_left_of_cursor");
		spec.replxx.bind_key_internal(Replxx::KEY::DELETE, "delete_character_under_cursor");
		spec.replxx.bind_key_internal(Replxx::KEY::LEFT, "move_cursor_left");
		spec.replxx.bind_key_internal(Replxx::KEY::RIGHT, "move_cursor_right");
		spec.replxx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::UP), "history_previous");
		spec.replxx.bind_key_internal(Replxx::KEY::meta(Replxx::KEY::DOWN), "history_next");
		spec.replxx.bind_key_internal(Replxx::KEY::PAGE_UP, "history_first");
		spec.replxx.bind_key_internal(Replxx::KEY::PAGE_DOWN, "history_last");
		spec.replxx.bind_key_internal(Replxx::KEY::HOME, "move_cursor_to_begining_of_line");
		spec.replxx.bind_key_internal(Replxx::KEY::END, "move_cursor_to_end_of_line");

		printNL(
			"\x1b[1mWelcome to \x1b[32mBeRD\x1b[0;1m - an interactive in-DVM debugger!\x1b[0m (now "
			"with replxx support!)"
		);

		spec.running        = true;
		spec.current_prompt = std::format(PROMPT_TEMPLATE, typeToString(debugger.getStatus()));

		events::Listener<api::ProcStatus> status_change_listener([&](const api::ProcStatus& status) {
			std::stringstream sstr;
			sstr << typeToString(status);
			if (v_matches(status, vm::api::ExecutionCompleted)) {
				const auto& exit_value = std::get<vm::api::ExecutionCompleted>(status).exit_value;
				if (v_matches(exit_value, std::vector<Ref<vm::IVMValue>>)) {
					for (auto val: std::get<std::vector<Ref<vm::IVMValue>>>(exit_value)) {
						if_opt_some(val->readData(), data) {
							variant_match(data) {
								variant_case(vm::interpreted_data_variant::Primitive, primitive) {
									sstr << "(" << std::to_string(primitive.value) << ")";
									// printNL("exit value: ", std::to_string(primitive.value));
								}
							}
						}
					}
				}
			}
			spec.current_prompt = std::format(PROMPT_TEMPLATE, sstr.str());
			spec.replxx.set_prompt(spec.current_line + spec.current_prompt);
		});

		events::Listener<std::string> error_listener([&](const std::string& err) {
			printError(err);
		});

		events::Listener<std::string> output_listener([&](const std::string& str) {
			print({ { str, printer::Color::BrightCyan } });
		});

		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnErrorListener(error_listener);
		debugger.attachOnOutputListener(output_listener);

		auto getline = [&](std::string& line) {
			const char* cinput{ nullptr };

			spec.is_prompt_active = true;

			do cinput = spec.replxx.input(spec.current_line + spec.current_prompt);
			while ((cinput == nullptr) && (errno == EAGAIN));

			spec.is_prompt_active = false;
			if (cinput == nullptr) return false;

			line = cinput;

			spec.replxx.history_add(line);
			return true;
		};

		for (std::string line; spec.running && getline(line); clah.execute(strip(line)));

		if (spec.running) printNL("exit");
		spec.running = false;

		status_change_listener.detach();

		printNL("Exiting debugger.");
		spec.replxx.invoke(Replxx::ACTION::CLEAR_SELF, 0);

		spec.replxx.history_sync(HISTORY_FILE_PATH);
		spec.replxx.disable_bracketed_paste();
	}

	void CLIDebugger::exitMainLoop() { spec.running = false; }

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		// a lot of copying for now, to prevent replxx from eating every thing printed without newline

		std::stringstream sstr;
		printer::StreamPrinter::print(content, sstr);
		std::string content_string = sstr.str();

		std::lock_guard _(spec.current_line_mutex);

		std::string before_content = spec.current_line;

		spec.current_line = spec.current_line + content_string;
		if (std::size_t pos = spec.current_line.find_last_of('\n'); pos != std::string::npos)
			spec.current_line = spec.current_line.substr(pos + 1);

		// we call Replxx::print which is apparently c-style
		// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
		if (spec.is_prompt_active) {
			spec.replxx.invoke(Replxx::ACTION::CLEAR_SELF, 0);
			spec.replxx.print(before_content.c_str());
			spec.replxx.print(content_string.c_str());
			spec.replxx.set_prompt(spec.current_line + spec.current_prompt);
			spec.replxx.invoke(Replxx::ACTION::REPAINT, 0);
		} else {
			spec.replxx.print(content_string.c_str());
		}
		// NOLINTEND(cppcoreguidelines-pro-type-vararg)
	}
}
