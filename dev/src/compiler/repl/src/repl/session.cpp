#include "session.hpp"

#include <driver/repl_utils/repl_dvm_helpers.hpp>
#include <driver/repl_utils/repl_split_helpers.hpp>
#include <driver/repl_utils/repl_statement_helpers.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
// @TODO: #1824 Move platform dependent includes to a separate file.
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>

#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <vm/api/vm.hpp>

#include <cstring>
#include <iostream>
#include <string_view>

namespace compiler::repl {
	// @TODO: #1784 decide if we want to do it here or in the main.cpp.
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
		  m_should_exit(false),
		  m_line_counter(0),
		  m_dvm_pid(0),
		  m_frontend(),
		  m_lowering_context() {
		initDVM();
	}

	bool ReplSession::isCommand(std::string_view line) const {
		return !line.empty() && line[0] == '/';
	}

	frontend::ModuleID ReplSession::getCurrentModuleID() const {
		CORE_ASSERT(!m_history.empty(), "No current REPL module available");
		return m_history.back().module_id;
	}

	ReplResult ReplSession::executeSingleStatement(frontend::ModuleID module_id) {
		base::Optional<SingleStatementInfo> stmt_info;
		std::string                         classify_error;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto classify_result = classifySingleStatement(ctx, module_id);
			if (!classify_result.has_value()) {
				classify_error = classify_result.error();
				return;
			}
			stmt_info = classify_result.value();
		});

		if (!stmt_info.has_value()) return ReplResult::error(classify_error);

		if (std::holds_alternative<ExpressionSingleStatementInfo>(stmt_info.value())) {
			auto expr_info = std::get<ExpressionSingleStatementInfo>(stmt_info.value());
			CORE_DEV_LOG(REPL, "Processing statement as expression\n");
			return handleExpression(expr_info.expr_stmt);
		}

		if (std::holds_alternative<InstructionSingleStatementInfo>(stmt_info.value())) {
			auto instruction_info = std::get<InstructionSingleStatementInfo>(stmt_info.value());
			CORE_DEV_LOG(REPL, "Processing statement as instruction\n");
			return handleInstruction(instruction_info.instruction_stmt);
		}

		CORE_DEV_LOG(REPL, "Processing statement as definition\n");
		auto definition_info = std::get<DefinitionSingleStatementInfo>(stmt_info.value());
		return handleDefinition(definition_info.definition_stmt);
	}

	bool ReplSession::handleCommand(std::string_view line) {
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
			clearTerminal();
			std::cout << "clean terminal\n";
			return true;
		}

		std::cerr << "Unknown command: " << line << "\n";
		std::cerr << "Type /help to see available commands.\n";
		return false;
	}

	void ReplSession::clearTerminal() {}

	void ReplSession::clearHistory() {
		m_frontend.clearHistory();
		m_history.clear();
		m_line_counter = 0;
	}

	ReplResult ReplSession::processLine(std::string_view line) {
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
			try {
				auto build_result = buildStatementWrapper(
					ctx,
					SingleStatementInfo{ ExpressionSingleStatementInfo{ .expr_stmt = expr_stmt } },
					m_line_counter
				);
				if (!build_result.has_value()) {
					error_message = build_result.error();
					had_error     = true;
					return;
				}

				expr_wrapper      = std::move(build_result->wrapper_function);
				wrapper_func_name = std::move(build_result->wrapper_func_name);
				CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
			} catch (const base::Panic& e) {
				error_message = "Expression evaluation failed: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			} catch (const std::exception& e) {
				error_message
					= "Unexpected error during expression evaluation: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			}
		});

		if (had_error) return ReplResult::error(error_message);

		CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
		auto hout_unit = makeExecutableHOUTUnit(expr_wrapper.value());

		query::utils::withContextDo([&](query::Context& ctx) {
			// Initialize context on first use or update it for this scope
			if (!m_lowering_context.has_value()) m_lowering_context.emplace(ctx);
			m_lowering_context->setContext(ctx);  // Update context for this scope

			// Defer: invalidate when exiting this scope, even on early return
			defer(m_lowering_context->invalidateContext());

			try {
				std::stringstream ss;
				hout_unit.debugPrint(ctx, ss);
				CORE_DEV_LOG(REPL, "HOUT unit:\n", ss.str(), "\n");

				CORE_DEV_LOG(REPL, "Compiling and loading to DVM\n");
				auto eval_module_id = getCurrentModuleID();
				auto module_name    = getStatementModuleName(eval_module_id);
				auto load_result    = compileAndLoad(
                    ctx, hout_unit, module_name, m_dvm_pid, m_lowering_context.value()
                );
				if (!load_result.has_value()) {
					error_message = "DVM load error: " + load_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}

				CORE_DEV_LOG(REPL, "Expression compiled and loaded to DVM\n");

				auto return_type = hout_unit.functions[0]->declaration->return_type;
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
			} catch (const base::Panic& e) {
				error_message = "Compilation/execution error: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			} catch (const std::exception& e) {
				error_message = "Unexpected error: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			}
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::handleInstruction(const pst::AccessLocked<pst::Stmt>& stmt) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;

		base::Optional<helios::HOUTFunction> instr_wrapper;
		std::string                          wrapper_func_name;

		CORE_DEV_LOG(REPL, "Starting handleInstruction\n");

		query::utils::withContextDo([&](query::Context& ctx) {
			try {
				auto build_result = buildStatementWrapper(
					ctx,
					SingleStatementInfo{
						InstructionSingleStatementInfo{ .instruction_stmt = stmt } },
					m_line_counter
				);
				if (!build_result.has_value()) {
					error_message = build_result.error();
					had_error     = true;
					return;
				}

				instr_wrapper     = std::move(build_result->wrapper_function);
				wrapper_func_name = std::move(build_result->wrapper_func_name);
				CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
			} catch (const base::Panic& e) {
				error_message = "Instruction evaluation failed: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			} catch (const std::exception& e) {
				error_message
					= "Unexpected error during instruction evaluation: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			}
		});

		if (had_error) return ReplResult::error(error_message);

		CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
		auto hout_unit = makeExecutableHOUTUnit(instr_wrapper.value());

		query::utils::withContextDo([&](query::Context& ctx) {
			// Initialize context on first use or update it for this scope
			if (!m_lowering_context.has_value()) m_lowering_context.emplace(ctx);
			m_lowering_context->setContext(ctx);  // Update context for this scope

			// Defer: invalidate when exiting this scope, even on early return
			defer(m_lowering_context->invalidateContext());

			try {
				std::stringstream ss;
				hout_unit.debugPrint(ctx, ss);
				CORE_DEV_LOG(REPL, "HOUT unit:\n", ss.str(), "\n");

				CORE_DEV_LOG(REPL, "Compiling and loading to DVM\n");
				auto eval_module_id = getCurrentModuleID();
				auto module_name    = getStatementModuleName(eval_module_id);
				auto load_result    = compileAndLoad(
                    ctx, hout_unit, module_name, m_dvm_pid, m_lowering_context.value()
                );
				if (!load_result.has_value()) {
					error_message = "DVM load error: " + load_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}

				CORE_DEV_LOG(REPL, "Instruction compiled and loaded to DVM\n");

				// Instructions always return unit — run and join without reading an exit value.
				auto run_result = vm::api::runFunction(m_dvm_pid, wrapper_func_name, {})
				                      .and_then([&](auto) { return vm::api::join(m_dvm_pid); })
				                      .transform_error(vm::api::errorToString);
				if (run_result.has_value()) {
					std::cout << "Instruction executed.\n";
				} else {
					error_message = "Runtime error: " + run_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}
			} catch (const base::Panic& e) {
				error_message = "Compilation/execution error: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			} catch (const std::exception& e) {
				error_message = "Unexpected error: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			}
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::handleDefinition(const pst::AccessLocked<pst::Stmt>& stmt) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;
		auto        module_id = getCurrentModuleID();

		query::utils::withContextDo([&](query::Context& ctx) {
			// Initialize context on first use or update it for this scope
			if (!m_lowering_context.has_value()) m_lowering_context.emplace(ctx);
			m_lowering_context->setContext(ctx);  // Update context for this scope

			// Defer: invalidate when exiting this scope, even on early return
			defer(m_lowering_context->invalidateContext());

			try {
				const auto& hout_unit = getDefinitionHOUTUnit(ctx, module_id);
				// The stmt parameter is used only here, for logging. It's not needed for the actual
				// query since QueryModuleHOUT already compiles the entire module containing the
				// statement.
				auto stmt_kind = stmt.unlock(ctx)->getElementKind();
				CORE_DEV_LOG(REPL, "Definition statement kind: ", static_cast<u32>(stmt_kind), "\n");
				std::stringstream ss;
				hout_unit.debugPrint(ctx, ss);
				CORE_DEV_LOG(REPL, "HOUT unit:\n", ss.str(), "\n");

				auto module_name = getStatementModuleName(module_id);
				auto load_result = compileAndLoad(
					ctx, hout_unit, module_name, m_dvm_pid, m_lowering_context.value()
				);
				if (!load_result.has_value()) {
					error_message = "DVM load error: " + load_result.error();
					std::cerr << error_message << "\n";
					had_error = true;
					return;
				}

				CORE_DEV_LOG(REPL, "Definitions loaded.\n");
			} catch (const base::Panic& e) {
				error_message = "Definition compilation error: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			} catch (const std::exception& e) {
				error_message = "Unexpected error: " + std::string(e.what());
				std::cerr << error_message << "\n";
				had_error = true;
			}
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::executeInput(std::string_view input) {
		if (input.empty()) return ReplResult::success();

		try {
			CORE_DEV_LOG(REPL, "Starting executeInput\n");

			CORE_DEV_LOG(REPL, "Parsing input for statement extraction\n");

			match_optional(splitInputIntoStatements(input)) {
				opt_some(statement_sources) {
					if (statement_sources.empty()) return ReplResult::success();

					CORE_DEV_LOG(
						REPL, "Input divided into ", statement_sources.size(), " statement(s):\n"
					);
					for (usize i = 0; i < statement_sources.size(); ++i)
						CORE_DEV_LOG(REPL, "  [", i + 1, "] \"", statement_sources[i], "\"\n");

					CORE_DEV_LOG(REPL, "Executing ", statement_sources.size(), " statement(s)\n");

					ReplResult last_result = ReplResult::success();
					for (const auto& stmt_source: statement_sources) {
						CORE_DEV_LOG(REPL, "Executing statement: \"", stmt_source, "\"\n");
						base::Optional<frontend::ModuleID> parent_module_id;
						if (!m_history.empty()) {
							parent_module_id = m_history.back().module_id;
							CORE_DEV_LOG(
								REPL,
								"Setting REPL parent to module #",
								m_history.back().module_id.queryUnstablePerfectHash(),
								"\n"
							);
						} else {
							CORE_DEV_LOG(REPL, "First REPL module, no parent\n");
						}

						auto module_ref = createEphemeralChainedStatementModule(
							stmt_source, parent_module_id, m_line_counter, "repl_"
						);
						auto module_id = module_ref->getModuleID();

						CORE_DEV_LOG(REPL, "Creating module\n");
						CORE_DEV_LOG(
							REPL,
							"Module created: #",
							module_id.queryUnstablePerfectHash(),
							", isRepl=",
							module_ref->isReplModule(),
							", hasParent=",
							module_ref->getReplModuleParent().has_value(),
							"\n"
						);
						m_history.emplace_back(stmt_source, module_id);
						++m_line_counter;
						last_result = executeSingleStatement(module_id);
						if (last_result.status == ReplResult::Status::Error) return last_result;
					}
					return last_result;
				}
				opt_err(err) return ReplResult::error(err);
			}
			CORE_UNREACHABLE();

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
