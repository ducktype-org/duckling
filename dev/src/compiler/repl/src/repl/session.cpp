#include "session.hpp"

#include <driver/repl_utils/repl_dvm_helpers.hpp>
#include <driver/repl_utils/repl_split_helpers.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/repl_utils/repl_queries.hpp>
// @TODO: #1824 Move platform dependent includes to a separate file.
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <base/except/exceptions.hpp>

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
		  m_inputs_counter(0),
		  m_dvm_pid(0),
		  m_frontend() {
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
		base::Optional<pst::AccessLocked<pst::ExprStmt>> expr_stmt_opt;
		base::Optional<pst::AccessLocked<pst::Stmt>>     instr_stmt_opt;
		base::Optional<pst::AccessLocked<pst::Stmt>>     top_level_stmt_opt;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
			auto pst       = getFilePST(ctx, main_file);
			auto root      = pst->getRootElement();
			expr_stmt_opt  = pst::extractSingleExpression(ctx, root);
			if (!expr_stmt_opt.has_value())
				instr_stmt_opt = pst::extractSingleInstruction(ctx, root);
			if (!expr_stmt_opt.has_value() && !instr_stmt_opt.has_value())
				top_level_stmt_opt = pst::extractSingleTopLevelStatement(ctx, root);
		});

		if (expr_stmt_opt.has_value()) {
			CORE_DEV_LOG(REPL, "Processing statement as expression\n");
			return handleExpression(expr_stmt_opt.value());
		}

		if (instr_stmt_opt.has_value()) {
			CORE_DEV_LOG(REPL, "Processing statement as instruction\n");
			return handleInstruction(instr_stmt_opt.value());
		}

		CORE_DEV_LOG(REPL, "Processing statement as definition\n");
		CORE_ASSERT(top_level_stmt_opt.has_value(), "Expected a single top-level statement");
		return handleDefinition(top_level_stmt_opt.value());
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
			clearHistory();
			std::cout << "History cleared.\n";
			return true;
		}

		std::cerr << "Unknown command: " << line << "\n";
		std::cerr << "Type /help to see available commands.\n";
		return false;
	}

	void ReplSession::clearHistory() {
		m_frontend.clearHistory();
		m_history.clear();
		m_inputs_counter = 0;
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
			CORE_DEV_LOG(REPL, "Querying QueryReplExpressionWrapper\n");
			expr_wrapper = ctx.query<QueryReplExpressionWrapper>({ .expr_stmt = expr_stmt,
			                                                       .counter   = m_inputs_counter });

			CORE_DEV_LOG(REPL, "Getting mangled name\n");
			auto mangled_name = helios::mangler::getSimpleMangledName(
				ctx, expr_wrapper->declaration->original_symbol
			);
			wrapper_func_name = mangled_name.strView();
			CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
		});

		CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
		helios::HOUTUnit hout_unit;
		hout_unit.functions.emplace_back(&expr_wrapper.value());

		query::utils::withContextDo([&](query::Context& ctx) {
			CORE_DEV_LOG(REPL, "HOUT unit:\n", hout_unit.debugPrint(ctx), "\n");

			CORE_DEV_LOG(REPL, "Compiling and loading to DVM\n");
			auto eval_module_id = getCurrentModuleID();
			auto module_name    = base::StrID(
                base::strConcat(
                    "repl_module_",
                    frontend::ModuleTree::getPathComponentHash(eval_module_id).hash.toStringHex()
                )
                    .c_str()
            );
			auto load_result = compileAndLoad(ctx, hout_unit, module_name.strView(), m_dvm_pid);
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
			CORE_DEV_LOG(REPL, "Querying QueryReplInstructionWrapper\n");
			instr_wrapper = ctx.query<QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = m_inputs_counter,
			});

			CORE_DEV_LOG(REPL, "Getting mangled name\n");
			auto mangled_name = helios::mangler::getSimpleMangledName(
				ctx, instr_wrapper->declaration->original_symbol
			);
			wrapper_func_name = mangled_name.strView();
			CORE_DEV_LOG(REPL, "Wrapper function name: ", wrapper_func_name, "\n");
		});

		CORE_DEV_LOG(REPL, "Creating HOUT unit\n");
		helios::HOUTUnit hout_unit;
		hout_unit.functions.emplace_back(&instr_wrapper.value());

		query::utils::withContextDo([&](query::Context& ctx) {
			CORE_DEV_LOG(REPL, "HOUT unit:\n", hout_unit.debugPrint(ctx), "\n");

			CORE_DEV_LOG(REPL, "Compiling and loading to DVM\n");
			auto eval_module_id = getCurrentModuleID();
			auto module_name    = base::StrID(
                base::strConcat(
                    "repl_module_",
                    frontend::ModuleTree::getPathComponentHash(eval_module_id).hash.toStringHex()
                )
                    .c_str()
            );
			auto load_result = compileAndLoad(ctx, hout_unit, module_name.strView(), m_dvm_pid);
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
		});

		if (had_error) return ReplResult::error(error_message);

		return ReplResult::success(output_message);
	}

	ReplResult ReplSession::handleDefinition(const pst::AccessLocked<pst::Stmt>& stmt) {
		std::string output_message;
		std::string error_message;
		bool        had_error = false;
		auto        module_id = getCurrentModuleID();

		const auto& hout_unit
			= query::entryPoint<helios::QueryModuleHOUT>(module_id)->valueOrThrow();

		query::utils::withContextDo([&](query::Context& ctx) {
			// The stmt parameter is used only here, for logging. It's not needed for the actual query
			// since QueryModuleHOUT already compiles the entire module containing the statement.
			auto stmt_kind = stmt.unlock(ctx)->getElementKind();
			CORE_DEV_LOG(REPL, "Definition statement kind: ", static_cast<u32>(stmt_kind), "\n");
			CORE_DEV_LOG(REPL, "HOUT unit:\n", hout_unit.debugPrint(ctx), "\n");

			auto module_name = base::StrID(
				base::strConcat(
					"repl_module_",
					frontend::ModuleTree::getPathComponentHash(module_id).hash.toStringHex()
				)
					.c_str()
			);
			auto load_result = compileAndLoad(ctx, hout_unit, module_name.strView(), m_dvm_pid);
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

	/**
	 * @brief Create a new REPL module with proper parent linkage.
	 *
	 * @param input The source code to compile into a module
	 * @param history The REPL history containing previously created modules
	 * @param line_counter The current REPL statement counter
	 * @return A reference to the newly created module tree
	 */
	static base::Ref<frontend::ModuleTree> createReplModule(
		const std::string& input, const std::vector<ReplStatement>& history, u64 line_counter
	) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::generateRandomString(32));

		auto virtual_file = fs::FileManager::createRandomVirtualFile(input);
		builder->setMainSourceFile(virtual_file);

		std::string module_name = "repl_" + std::to_string(line_counter);
		CORE_DEV_LOG(
			REPL, "Creating module with name: ", module_name, " (counter=", line_counter, ")\n"
		);

		builder->setName(base::StrID(module_name.c_str()));

		// Create ReplData with optional parent linkage
		frontend::ReplData repl_data;
		if (!history.empty()) {
			auto last_module_id = history.back().module_id;
			CORE_DEV_LOG(
				REPL,
				"Setting REPL parent to module #",
				last_module_id.queryUnstablePerfectHash(),
				"\n"
			);
			repl_data.m_repl_module_parent = last_module_id;
		} else {
			CORE_DEV_LOG(REPL, "First REPL module, no parent\n");
		}

		builder->setReplModule(repl_data);

		return builder->finalize();
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
						auto module_ref
							= createReplModule(stmt_source, m_history, m_inputs_counter);
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
						++m_inputs_counter;
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
