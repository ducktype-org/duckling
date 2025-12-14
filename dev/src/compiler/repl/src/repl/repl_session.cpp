/**
 * @file repl_session.cpp
 * @brief Implementation of REPL session management for Duckling compiler.
 */

#include "repl_session.hpp"

#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <iostream>

namespace compiler::repl {

	ReplSession::ReplSession():
		  m_config(),
		  m_accumulated_input(),
		  m_should_exit(false),
		  m_line_counter(0) {}

	ReplSession::ReplSession(ReplConfig config):
		  m_config(std::move(config)),
		  m_accumulated_input(),
		  m_should_exit(false),
		  m_line_counter(0) {}

	void ReplSession::printWelcome() const {
		std::cout << "Duckling REPL\n";
		std::cout << "Type /help for available commands, /exit to quit.\n";
		std::cout << "Enter " << m_config.multiline_start << " for multiline mode.\n\n";
	}

	void ReplSession::printPrompt() const {
		if (m_accumulated_input.empty())
			std::cout << m_config.prompt;
		else
			std::cout << m_config.continuation;
		std::cout.flush();
	}

	bool ReplSession::isCommand(const std::string& line) const {
		return !line.empty() && line[0] == '/';
	}

	bool ReplSession::handleCommand(const std::string& line) {
		if (line == "/exit" || line == "/quit" || line == "/q") {
			m_should_exit = true;
			return true;
		}

		if (line == "/history" || line == "/h") {
			printHistory();
			return true;
		}

		if (line == "/help" || line == "/?") {
			printHelp();
			return true;
		}

		if (line == "/clear" || line == "/c") {
			clearHistory();
			std::cout << "History cleared.\n";
			return true;
		}

		std::cerr << "Unknown command: " << line << "\n";
		std::cerr << "Type /help to see available commands.\n";
		return false;
	}

	void ReplSession::printHistory() const {
		if (m_history.empty()) {
			std::cout << "No history yet.\n";
			return;
		}

		std::cout << "\n=== REPL History (" << m_history.size()
				  << (m_history.size() == 1 ? " statement" : " statements") << ") ===\n";
		for (size_t i = 0; i < m_history.size(); ++i) {
			const auto& stmt = m_history[i];
			std::cout << "[" << (i + 1) << "] ";

			if (stmt.source_code.find('\n') != std::string::npos) {
				std::cout << "(multiline)\n";
				std::cout << stmt.source_code << "\n";
			} else {
				std::cout << stmt.source_code << "\n";
			}
		}
		std::cout << "\n";
	}

	void ReplSession::printHelp() const {
		std::cout << "\n=== REPL Commands ===\n";
		std::cout << "  /help, /?           - Show this help message\n";
		std::cout << "  /exit, /quit, /q    - Exit the REPL\n";
		std::cout << "  /history, /h        - Show all executed statements\n";
		std::cout << "  /clear, /c          - Clear statement history\n";
		std::cout << "\n=== Multiline Mode ===\n";
		std::cout << "  " << m_config.multiline_start
				  << "                  - Start multiline input\n";
		std::cout << "  " << m_config.multiline_end
				  << "                   - End multiline input and execute\n";
		std::cout << "\n";
	}

	void ReplSession::clearHistory() {
		m_history.clear();
		m_line_counter = 0;
	}

	std::string ReplSession::handleMultilineInput() {
		std::string multiline_input;

		std::cout << "(Multiline mode - type '" << m_config.multiline_end
				  << "' on a new line to finish)\n";

		while (true) {
			std::cout << "         |";
			std::cout.flush();

			std::string ml;
			if (!std::getline(std::cin, ml)) break;

			if (ml == m_config.multiline_end) break;

			if (!multiline_input.empty()) multiline_input += "\n";
			multiline_input += ml;
		}

		return multiline_input;
	}

	ReplResult ReplSession::processLine(const std::string& line) {
		if (m_accumulated_input.empty() && isCommand(line)) {
			handleCommand(line);
			if (m_should_exit) return ReplResult::exit();
			return ReplResult::success();
		}

		if (line == m_config.multiline_start) {
			std::string multiline_content = handleMultilineInput();

			if (!m_accumulated_input.empty()) m_accumulated_input += "\n";
			m_accumulated_input += multiline_content;

			auto result = executeInput(m_accumulated_input);
			m_accumulated_input.clear();
			return result;
		}

		if (!m_accumulated_input.empty()) m_accumulated_input += "\n";
		m_accumulated_input += line;

		auto result = executeInput(m_accumulated_input);
		m_accumulated_input.clear();
		return result;
	}

	ReplResult ReplSession::executeInput(const std::string& input) {
		try {
			auto module_ref = frontend::ModuleTreeBuilder::createFromContents(input);
			auto module_id  = module_ref->getModuleID();

			m_history.emplace_back(input, module_ref);
			++m_line_counter;

			auto hout_unit = query::entryPoint<helios::QueryReplStatementTo>(module_id);

			std::string output_message;
			i32         dvm_exit_code = 0;

			query::utils::withContextDo(
				[&](query::Context& ctx) {
					if (m_config.show_hout_debug) {
						auto hout_debug = hout_unit.debugPrint(ctx);
						std::cout << hout_debug << "\n";
						output_message = hout_debug;
					}

					if (m_config.run_dvm) {
						std::string hout_debug = hout_unit.debugPrint(ctx);

						if (hout_debug.find("fun main") == std::string::npos) {
							std::cout
								<< "No 'main' function found in module; skipping DVM execution.\n";
						} else {
							auto run_result = driver::runModuleOnDVM(ctx, module_id);
							if (run_result.has_value()) {
								dvm_exit_code = run_result.value().exit_code;
								std::cout << "DVM run exit code: " << dvm_exit_code << "\n";
							} else {
								std::cerr << "DVM run error: " << run_result.error() << "\n";
								return;
							}
						}
					}
				}
			);

			return ReplResult::success(output_message, dvm_exit_code);

		} catch (const std::exception& e) {
			std::string error_msg = std::string("REPL processing exception: ") + e.what();
			std::cerr << error_msg << "\n";
			return ReplResult::error(error_msg);
		} catch (...) {
			std::string error_msg = "Unknown REPL processing error";
			std::cerr << error_msg << "\n";
			return ReplResult::error(error_msg);
		}
	}

	int ReplSession::run() {
		printWelcome();

		std::string line;
		while (!m_should_exit) {
			printPrompt();

			if (!std::getline(std::cin, line)) {
				std::cout << "\nGoodbye!\n";
				break;
			}

			auto result = processLine(line);

			if (result.status == ReplResult::Status::Exit) {
				std::cout << result.message << "\n";
				break;
			}
		}

		return 0;
	}

}  // namespace compiler::repl
