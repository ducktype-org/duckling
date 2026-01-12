#include "repl_session.hpp"

#include "driver/repl_utils/repl_dvm_helpers.hpp"

#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries.hpp>
#include <helios/repl_utils/repl_queries.hpp>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <base/except/exceptions.hpp>

#include <logger/logger.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <vm/api/vm.hpp>

#include <cstring>
#include <iostream>
#include <string_view>

namespace compiler::repl {
	// @TODO ##1784: decide if we want to do it here or in the main.cpp.
	void ReplSession::initDVM() {
		auto spawn_result = vm::api::spawn();
		CORE_ASSERT(spawn_result.has_value(), "ReplSession::initDVM: Failed to spawn DVM process");

		m_dvm_pid = spawn_result->pid;

		auto attach_result = vm::api::attach(m_dvm_pid, std::cin, std::cout);
		CORE_ASSERT(
			attach_result.has_value(), "ReplSession::initDVM: Failed to attach I/O to DVM process"
		);

		CORE_DEV_LOG(REPL, "DVM initialized with PID ", m_dvm_pid, "\n");
	}

	ReplSession::ReplSession():
		  m_config(),
		  m_should_exit(false),
		  m_line_counter(0),
		  m_dvm_pid(0),
		  m_frontend(m_history, m_config) {
		initDVM();
	}

	ReplSession::ReplSession(ReplConfig config):
		  m_config(std::move(config)),
		  m_should_exit(false),
		  m_line_counter(0),
		  m_dvm_pid(0),
		  m_frontend(m_history, m_config) {
		initDVM();
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
			m_frontend.printHistory();
			return true;
		}

		if (line == "/help" || line == "/?") {
			m_frontend.printHelp();
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

	void ReplSession::clearHistory() {
		m_history.clear();
		m_line_counter = 0;
	}

	ReplResult ReplSession::processLine(const std::string& line) {
		if (isCommand(line)) {
			handleCommand(line);
			if (m_should_exit) return ReplResult::exit();
			return ReplResult::success();
		}

		return executeInput(line);
	}

	ReplResult ReplSession::handleExpression(const pst::AccessLocked<pst::ExprStmt>& expr_stmt) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;

		base::Optional<helios::HOUTFunction> expr_wrapper;
		std::string                          wrapper_func_name;

		CORE_DEV_LOG(REPL, "Starting handleExpression\n");

		query::utils::withContextDo([&](query::Context& ctx) {
			CORE_DEV_LOG(REPL, "Querying QueryReplExpressionWrapper\n");
			expr_wrapper = ctx.query<QueryReplExpressionWrapper>({ .expr_stmt = expr_stmt,
			                                                       .counter   = m_line_counter });

			CORE_DEV_LOG(REPL, "Getting mangled name\n");
			auto mangled_name = helios::mangler::getSimpleMangledName(
				ctx, expr_wrapper->declaration->original_symbol
			);
			wrapper_func_name = mangled_name.strView();
			CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
		});

		CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
		helios::HOUTUnit hout_unit;
		hout_unit.functions.push_back(std::move(expr_wrapper.value()));

		query::utils::withContextDo([&](query::Context& ctx) {
			CORE_DEV_LOG(REPL, "HOUT unit:\n", hout_unit.debugPrint(ctx), "\n");

			CORE_DEV_LOG(REPL, "Compiling and loading to DVM\n");
			auto load_result = compileAndLoad(ctx, hout_unit, m_dvm_pid);
			if (!load_result.has_value()) {
				error_message = "DVM load error: " + load_result.error();
				std::cerr << error_message << "\n";
				had_error = true;
				return;
			}

			CORE_DEV_LOG(REPL, "Expression compiled and loaded to DVM\n");

			auto return_type = hout_unit.functions[0].declaration->return_type;
			auto run_result
				= executeFunctionAndCaptureResult(m_dvm_pid, wrapper_func_name, return_type);
			if (run_result.has_value()) {
				if (return_type.toString() == "void")
					std::cout << "Function executed.\n";
				else
					std::cout << "=> " << run_result.value() << "\n";
			} else {
				error_message = "Runtime error: " + run_result.error();
				std::cerr << error_message << "\n";
				had_error = true;
				return;
			}
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::handleDefinition(frontend::ModuleID module_id) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;

		auto hout_unit = query::entryPoint<helios::QueryModuleHOUT>(module_id).valueOrThrow();

		query::utils::withContextDo([&](query::Context& ctx) {
			CORE_DEV_LOG(REPL, "HOUT unit:\n", hout_unit.debugPrint(ctx), "\n");

			auto load_result = compileAndLoad(ctx, hout_unit, m_dvm_pid);
			if (!load_result.has_value()) {
				error_message = "DVM load error: " + load_result.error();
				std::cerr << error_message << "\n";
				had_error = true;
				return;
			}

			std::cout << "Definitions loaded.\n";
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::executeInput(const std::string& input) {
		if (input.empty()) return ReplResult::success();

		try {
			CORE_DEV_LOG(REPL, "Starting executeInput\n");

			CORE_DEV_LOG(REPL, "Creating module\n");
			auto module_id = frontend::createModuleTreeFromContents(input);

			m_history.emplace_back(input, module_id);
			++m_line_counter;

			CORE_DEV_LOG(REPL, "Extracting expression\n");
			base::Optional<pst::AccessLocked<pst::ExprStmt>> expr_stmt_opt;
			query::utils::withContextDo([&](query::Context& ctx) {
				auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
				auto pst       = ctx.query<frontend::QueryFilePST>(main_file);

				CORE_DEV_LOG(REPL, "PST:\n");
				if (logger::isCategoryEnabled(logger::DevLogCategories::REPL)) {
					pst->dprint(std::cout);
					std::cout << "\n\n";
				}

				if (pst->getLogger()->bad()) {
					std::cerr << "Parse errors:\n";
					pst->getLogger()->dumpLog(false, std::cerr);
					return;
				}

				auto root     = pst->getRootElement();
				expr_stmt_opt = extractSingleExpression(ctx, root);
			});

			if (expr_stmt_opt.has_value()) {
				CORE_DEV_LOG(REPL, "Processing as expression\n");
				return handleExpression(expr_stmt_opt.value());
			} else {
				CORE_DEV_LOG(REPL, "Processing as definition\n");
				return handleDefinition(module_id);
			}
		} catch (const std::out_of_range& e) {
			std::string error_msg = std::string("REPL map::at error (out_of_range): ") + e.what();
			std::cerr << error_msg << "\n";
			CORE_DEV_LOG(REPL, "This typically means a lookup in a map/vector failed\n");
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
		m_frontend.printWelcome();
		while (!m_should_exit) {
			m_frontend.printPrompt();
			std::string line = m_frontend.readLine();

			if (line.empty() && std::cin.eof()) {
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
