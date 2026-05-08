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
#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <vm/api/vm.hpp>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace compiler::repl {
	namespace {}

	ReplResult ReplSession::failWithMessage(std::string_view message) {
		return ReplResult::error(std::string(message));
	}

	ReplResult ReplSession::runWithReplErrorHandling(
		std::string_view                   panic_prefix,
		std::string_view                   std_exception_prefix,
		const std::function<ReplResult()>& action
	) {
		try {
			return action();
		} catch (const base::Panic& e) {
			return ReplResult::error(base::strConcat(panic_prefix, e.what()));
		} catch (const std::exception& e) {
			return ReplResult::error(base::strConcat(std_exception_prefix, e.what()));
		} catch (...) {
			return ReplResult::error(base::strConcat(std_exception_prefix, "Unknown error occurred")
			);
		}
	}

	void ReplSession::runWithContextErrorHandling(
		std::string_view                            panic_prefix,
		std::string_view                            std_exception_prefix,
		const std::function<void(query::Context&)>& action,
		std::string&                                out_error
	) {
		query::utils::withContextDo([&](query::Context& ctx) {
			try {
				action(ctx);
			} catch (const base::Panic& e) {
				out_error = base::strConcat(panic_prefix, e.what());
			} catch (const std::exception& e) {
				out_error = base::strConcat(std_exception_prefix, e.what());
			} catch (...) {
				out_error = base::strConcat(std_exception_prefix, "Unknown error occurred");
			}
		});
	}

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

	ReplSession::ReplSession(bool completions_enabled):
		  m_should_exit(false),
		  m_line_counter(0),
		  m_dvm_pid(0),
		  m_frontend(completions_enabled),
		  m_lowering_context() {
		initDVM();
	}

	ReplResult ReplSession::loadScriptFile(std::string_view file_path) {
		auto trimmed_path = base::strTrim(file_path);
		if (trimmed_path.empty())
			return ReplResult::error("Missing script path. Usage: /load <path-to-script.ds>");

		try {
			m_suppress_repl_feedback_during_script_load = true;
			defer(m_suppress_repl_feedback_during_script_load = false);
			const fs::FilePath script_path{ std::string(trimmed_path) };
			fs::File           script_file{ script_path };
			auto               source = script_file.getContent().view().stdString();
			return executeInput(source);
		} catch (const std::exception& e) {
			return ReplResult::error(
				std::string("Failed to load script file '") + std::string(trimmed_path)
				+ "': " + e.what()
			);
		}
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
		auto command_end = line.find_first_of(" \t");
		auto command     = line.substr(0, command_end);
		auto args        = command_end == std::string_view::npos
		                     ? std::string_view{}
		                     : base::strTrimLeft(line.substr(command_end + 1));

		if (line == "/exit" || line == "/quit" || line == "/q") {
			m_should_exit = true;
			return true;
		}

		if (line == "/help" || line == "/?" || line == "/h") {
			m_frontend.printHelp();
			return true;
		}

		if (line == "/history" || line == "/hist") {
			m_frontend.printHistory();
			return true;
		}

		if (line == "/clear" || line == "/c") {
			m_frontend.clearScreen();
			return true;
		}

		if (command == "/load") {
			auto script_path = base::strTrim(args);
			auto load_result = loadScriptFile(script_path);
			if (load_result.status == ReplResult::Status::Error)
				std::cerr << load_result.message << "\n";
			else
				std::cout << "Script loaded: " << script_path << "\n";
			return true;
		}

		std::cerr << "Unknown command: " << line << "\n";
		std::cerr << "Type /help to see available commands.\n";
		return false;
	}

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
		return runWithReplErrorHandling(
			"Expression evaluation failed: ",
			"Unexpected error during expression evaluation: ",
			[&]() -> ReplResult {
				std::string output_message;
				std::string error_message;

				base::Optional<helios::HOUTFunction> expr_wrapper;
				std::string                          wrapper_func_name;

				CORE_DEV_LOG(REPL, "Starting handleExpression\n");

				runWithContextErrorHandling(
					"Expression evaluation failed: ",
					"Unexpected error during expression evaluation: ",
					[&](query::Context& ctx) {
						auto build_result = buildStatementWrapper(
							ctx,
							SingleStatementInfo{
								ExpressionSingleStatementInfo{ .expr_stmt = expr_stmt } },
							m_line_counter
						);
						if (!build_result.has_value()) {
							error_message = build_result.error();
							return;
						}

						expr_wrapper      = std::move(build_result->wrapper_function);
						wrapper_func_name = std::move(build_result->wrapper_func_name);
						CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
					},
					error_message
				);

				if (!error_message.empty()) return failWithMessage(error_message);

				CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
				auto hout_unit = makeExecutableHOUTUnit(expr_wrapper.value());

				runWithContextErrorHandling(
					"Compilation/execution error: ",
					"Unexpected error: ",
					[&](query::Context& ctx) {
						// Initialize context on first use or update it for this scope
						if (!m_lowering_context.has_value()) m_lowering_context.emplace(ctx);
						m_lowering_context->setContext(ctx);  // Update context for this scope

						// Defer: invalidate when exiting this scope, even on early return
						defer(m_lowering_context->invalidateContext());

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
							return;
						}

						CORE_DEV_LOG(REPL, "Expression compiled and loaded to DVM\n");

						auto return_type = hout_unit.functions[0]->declaration->return_type;
						auto run_result  = executeFunctionAndCaptureResult(
                            m_dvm_pid, wrapper_func_name, return_type
                        );
						if (run_result.has_value()) {
							if (!m_suppress_repl_feedback_during_script_load) {
								if (return_type.toString() == "()")
									std::cout << "Function executed.\n";
								else
									std::cout << "=> " << run_result.value() << "\n";
							}
						} else {
							error_message = "Runtime error: " + run_result.error();
							return;
						}
					},
					error_message
				);

				if (!error_message.empty()) return failWithMessage(error_message);

				return ReplResult::success(output_message);
			}
		);
	}

	ReplResult ReplSession::handleInstruction(const pst::AccessLocked<pst::Stmt>& stmt) {
		return runWithReplErrorHandling(
			"Instruction evaluation failed: ",
			"Unexpected error during instruction evaluation: ",
			[&]() -> ReplResult {
				std::string output_message;
				std::string error_message;

				base::Optional<helios::HOUTFunction> instr_wrapper;
				std::string                          wrapper_func_name;

				CORE_DEV_LOG(REPL, "Starting handleInstruction\n");

				runWithContextErrorHandling(
					"Instruction evaluation failed: ",
					"Unexpected error during instruction evaluation: ",
					[&](query::Context& ctx) {
						auto build_result = buildStatementWrapper(
							ctx,
							SingleStatementInfo{
								InstructionSingleStatementInfo{ .instruction_stmt = stmt } },
							m_line_counter
						);
						if (!build_result.has_value()) {
							error_message = build_result.error();
							return;
						}

						instr_wrapper     = std::move(build_result->wrapper_function);
						wrapper_func_name = std::move(build_result->wrapper_func_name);
						CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
					},
					error_message
				);

				if (!error_message.empty()) return failWithMessage(error_message);

				CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
				auto hout_unit = makeExecutableHOUTUnit(instr_wrapper.value());

				runWithContextErrorHandling(
					"Compilation/execution error: ",
					"Unexpected error: ",
					[&](query::Context& ctx) {
						// Initialize context on first use or update it for this scope
						if (!m_lowering_context.has_value()) m_lowering_context.emplace(ctx);
						m_lowering_context->setContext(ctx);  // Update context for this scope

						// Defer: invalidate when exiting this scope, even on early return
						defer(m_lowering_context->invalidateContext());

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
							return;
						}

						CORE_DEV_LOG(REPL, "Instruction compiled and loaded to DVM\n");

						// Instructions always return unit — run and join without reading an exit value.
						auto run_result
							= vm::api::runFunction(m_dvm_pid, wrapper_func_name, {})
				                  .and_then([&](auto) { return vm::api::join(m_dvm_pid); })
				                  .transform_error(vm::api::errorToString);
						if (run_result.has_value()) {
							if (!m_suppress_repl_feedback_during_script_load)
								std::cout << "Instruction executed.\n";
						} else {
							error_message = "Runtime error: " + run_result.error();
							return;
						}
					},
					error_message
				);

				if (!error_message.empty()) return failWithMessage(error_message);

				return ReplResult::success(output_message);
			}
		);
	}

	ReplResult ReplSession::handleDefinition(const pst::AccessLocked<pst::Stmt>& stmt) {
		return runWithReplErrorHandling(
			"Definition compilation error: ",
			"Unexpected error: ",
			[&]() -> ReplResult {
				std::string output_message;
				std::string error_message;
				auto        module_id = getCurrentModuleID();

				runWithContextErrorHandling(
					"Definition compilation error: ",
					"Unexpected error: ",
					[&](query::Context& ctx) {
						// Initialize context on first use or update it for this scope
						if (!m_lowering_context.has_value()) m_lowering_context.emplace(ctx);
						m_lowering_context->setContext(ctx);  // Update context for this scope

						// Defer: invalidate when exiting this scope, even on early return
						defer(m_lowering_context->invalidateContext());

						const auto& hout_unit = getDefinitionHOUTUnit(ctx, module_id);
						// The stmt parameter is used only here, for logging. It's not needed for
				        // the actual query since QueryModuleHOUT already compiles the entire module
				        // containing the statement.
						auto stmt_kind = stmt.unlock(ctx)->getElementKind();
						CORE_DEV_LOG(
							REPL, "Definition statement kind: ", static_cast<u32>(stmt_kind), "\n"
						);
						std::stringstream ss;
						hout_unit.debugPrint(ctx, ss);
						CORE_DEV_LOG(REPL, "HOUT unit:\n", ss.str(), "\n");

						auto module_name = getStatementModuleName(module_id);
						auto load_result = compileAndLoad(
							ctx, hout_unit, module_name, m_dvm_pid, m_lowering_context.value()
						);
						if (!load_result.has_value()) {
							error_message = "DVM load error: " + load_result.error();
							return;
						}

						CORE_DEV_LOG(REPL, "Definitions loaded.\n");
					},
					error_message
				);

				if (!error_message.empty()) return failWithMessage(error_message);

				return ReplResult::success(output_message);
			}
		);
	}

	ReplResult ReplSession::executeInput(std::string_view input) {
		return runWithReplErrorHandling("REPL execution failed: ", "Error: ", [&]() -> ReplResult {
			if (input.empty()) return ReplResult::success();

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
						if (last_result.status == ReplResult::Status::Error) {
							m_history.pop_back();
							return last_result;
						}
					}
					return last_result;
				}
				opt_err(err) { return ReplResult::error(err); }
			}
			CORE_UNREACHABLE();
		});
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
			if (result.status == ReplResult::Status::Error) {
				if (!result.message.empty()) std::cerr << result.message << "\n";
				continue;
			}

			if (result.status == ReplResult::Status::Success && !result.message.empty())
				std::cout << result.message << "\n";

			if (result.status == ReplResult::Status::Exit) {
				std::cout << result.message << "\n";
				break;
			}
		}

		return 0;
	}

}  // namespace compiler::repl
