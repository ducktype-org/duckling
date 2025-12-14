#include "repl_session.hpp"

#include "dvm_helpers.hpp"
#include "repl_queries.hpp"

#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <vm/api/vm.hpp>

#include <iostream>

namespace compiler::repl {
	// TODO: decide if we want to do it here or in the main.cpp.
	void ReplSession::initDVM() {
		auto spawn_result = vm::api::spawn();
		if (!spawn_result.has_value())
			throw base::Panic("ReplSession::initDVM", "Failed to spawn DVM process");

		m_dvm_pid = spawn_result->pid;

		auto attach_result = vm::api::attach(m_dvm_pid, std::cin, std::cout);
		if (!attach_result.has_value())
			throw base::Panic("ReplSession::initDVM", "Failed to attach I/O to DVM process");

		std::cout << "[DVM initialized with PID " << m_dvm_pid << "]\n";
	}

	ReplSession::ReplSession(): m_config(), m_should_exit(false), m_line_counter(0), m_dvm_pid(0) {
		initDVM();
	}

	ReplSession::ReplSession(ReplConfig config):
		  m_config(std::move(config)),
		  m_should_exit(false),
		  m_line_counter(0),
		  m_dvm_pid(0) {
		initDVM();
	}

	void ReplSession::printWelcome() const {
		std::cout << "Duckling REPL\n";
		std::cout << "Type /help for available commands, /exit to quit.\n";
		std::cout << "Enter " << m_config.multiline_start << " for multiline mode.\n\n";
	}

	void ReplSession::printPrompt() const {
		std::cout << m_config.prompt;
		std::cout.flush();
	}

	bool ReplSession::isCommand(const std::string& line) const {
		return !line.empty() && line[0] == '/';
	}

	// It doesn't belong here, should probably be moved to frontend later.
	base::Optional<pst::AccessLocked<pst::ExprStmt>> ReplSession::extractSingleExpression(
		query::Context& ctx, const pst::AccessLocked<pst::LangElement>& root
	) const {
		auto root_elem = root.unlock(ctx);
		auto children  = root_elem->viewChildren();

		auto it = children.begin();
		if (it == children.end()) return {};

		auto first_child = (*it).unlock(ctx);
		++it;

		if (it == children.end() && first_child->getElementKind() == pst::ElementKind::ExprStmt)
			return (*children.begin()).template dynamicCast<pst::ExprStmt>();

		return {};
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
		if (isCommand(line)) {
			handleCommand(line);
			if (m_should_exit) return ReplResult::exit();
			return ReplResult::success();
		}

		if (line == m_config.multiline_start) {
			std::string multiline_content = handleMultilineInput();
			return executeInput(multiline_content);
		}

		return executeInput(line);
	}

	ReplResult ReplSession::handleExpression(const pst::AccessLocked<pst::ExprStmt>& expr_stmt) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;

		base::Optional<helios::HOUTFunction> expr_wrapper;
		std::string                          wrapper_func_name;

		std::cout << "[DEBUG] Starting handleExpression\n";

		query::utils::withContextDo([&](query::Context& ctx) {
			std::cout << "[DEBUG] Querying QueryReplExpressionWrapper\n";
			expr_wrapper = ctx.query<QueryReplExpressionWrapper>({ .expr_stmt = expr_stmt,
			                                                       .counter   = m_line_counter });

			std::cout << "[DEBUG] Getting mangled name\n";
			auto mangled_name = helios::mangler::getSimpleMangledName(
				ctx, expr_wrapper->declaration->original_symbol
			);
			wrapper_func_name = mangled_name.strView();
			std::cout << "[DEBUG] Wrapper function name: " << wrapper_func_name << "\n";
		});

		std::cout << "[DEBUG] Creating HOUT unit\n";
		helios::HOUTUnit hout_unit;
		hout_unit.functions.push_back(std::move(expr_wrapper.value()));

		query::utils::withContextDo([&](query::Context& ctx) {
			if (m_config.show_hout_debug) {
				std::cout << "[DEBUG] Printing HOUT debug\n";
				auto hout_debug = hout_unit.debugPrint(ctx);
				std::cout << hout_debug << "\n";
				output_message = hout_debug;
			}

			if (m_config.run_dvm) {
				std::cout << "[DEBUG] Compiling and loading to DVM\n";
				auto load_result = compileAndLoad(ctx, hout_unit, m_dvm_pid);
				if (!load_result.has_value()) {
					error_message = "DVM load error: " + load_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}

				std::cout << "[Expression compiled and loaded to DVM]\n";

				auto return_type = hout_unit.functions[0].declaration->return_type;
				auto run_result  = executeExpression(m_dvm_pid, wrapper_func_name, return_type);
				if (run_result.has_value()) {
					if (return_type.toString() == "void")
						std::cout << "Function executed.\n";
					else
						std::cout << "=> " << run_result.value().result_string << "\n";
				} else {
					error_message = "Runtime error: " + run_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}
			}
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::handleDefinition(frontend::ModuleID module_id) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;

		auto hout_unit = query::entryPoint<repl::QueryReplModuleHOUT>(module_id);

		query::utils::withContextDo([&](query::Context& ctx) {
			if (m_config.show_hout_debug) {
				auto hout_debug = hout_unit.debugPrint(ctx);
				std::cout << hout_debug << "\n";
				output_message = hout_debug;
			}

			if (m_config.run_dvm) {
				auto load_result = compileAndLoad(ctx, hout_unit, m_dvm_pid);
				if (!load_result.has_value()) {
					error_message = "DVM load error: " + load_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}

				std::cout << "Definitions loaded.\n";
			}
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::executeInput(const std::string& input) {
		if (input.empty()) return ReplResult::success();

		try {
			std::cout << "[DEBUG] Starting executeInput\n";

			std::cout << "[DEBUG] Creating module\n";
			auto module_ref = frontend::ModuleTreeBuilder::createFromContents(input);
			auto module_id  = module_ref->getModuleID();

			m_history.emplace_back(input, module_ref);
			++m_line_counter;

			std::cout << "[DEBUG] Extracting expression\n";
			base::Optional<pst::AccessLocked<pst::ExprStmt>> expr_stmt_opt;
			query::utils::withContextDo([&](query::Context& ctx) {
				auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
				auto pst       = ctx.query<frontend::QueryFilePST>(main_file);

				pst->dprint(std::cout);
				std::cout << "\n\n";

				if (pst->getLogger()->bad()) {
					std::cerr << "Parse errors:\n";
					pst->getLogger()->dumpLog(false, std::cerr);
					return;
				}

				auto root     = pst->getRootElement();
				expr_stmt_opt = extractSingleExpression(ctx, root);
			});

			if (expr_stmt_opt.has_value()) {
				std::cout << "[DEBUG] Processing as expression\n";
				return handleExpression(expr_stmt_opt.value());
			} else {
				std::cout << "[DEBUG] Processing as definition\n";
				return handleDefinition(module_id);
			}
		} catch (const std::out_of_range& e) {
			std::string error_msg = std::string("REPL map::at error (out_of_range): ") + e.what();
			std::cerr << error_msg << "\n";
			std::cerr << "[DEBUG] This typically means a lookup in a map/vector failed\n";
			return ReplResult::error(error_msg);
		} catch (const std::exception& e) {
			std::string error_msg = std::string("Error: ") + e.what();
			std::cerr << error_msg << "\n";
			return ReplResult::error(error_msg);
		} catch (...) {
			std::string error_msg = "Unknown error occurred";
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
