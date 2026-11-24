// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/options.hpp>
#include <driver/repl_utils/script_helpers.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/script_context.hpp>
#include <linker/link.hpp>

#include <base/str/str_utils.hpp>

#include <diagnostic/logger.hpp>
#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

using namespace compiler;

class DriverScriptTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverScriptTest

	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(compilesBothBackendsWithoutLinking);
		TESTER_ADD_TEST(preservesDumpsAfterFailure);
		TESTER_ADD_TEST(runOnDVMPanicShowsScriptLines);
	}

protected:
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
		auto initialized = driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::ScriptMode{
				.script_file = fs::FileManager::createRandomVirtualFile(""),
				.backend_options = { .llvm_backend = global_state::BackendOptions::LLVMBackend{} },
				.compilation_artifacts = { .artifacts_path = artifacts_path },
				.debug_options = {
					.dev_log_categories = {},
					.dump_llvm = true, .dump_asm = true, .dump_dbc = true,
					.dump_lir = true, .dump_mir = true, .dump_hir = true,
				},
				.execution_options = { .worker_count = 1 },
				.stdlib_options = {},
			}
		);
		ASSERT_TRUE(initialized.status().isOk());
	}

	void afterAll() override {
		driver::exit();
		std::filesystem::remove_all(artifacts_path.getPath());
	}

private:
	void checkDumps(const std::string& module_name) {
		for (const auto* extension: { ".hir", ".mir", ".lir" }) {
			std::ifstream input(
				artifacts_path.getPath() / "duck_debug_artifacts" / (module_name + extension)
			);
			const std::string dump{ std::istreambuf_iterator<char>(input),
				                    std::istreambuf_iterator<char>() };
			assertTrue(
				dump.find("saved_foo_name") != std::string::npos,
				base::strConcat("Missing saved_foo_name function in ", extension)
			);
		}
	}

	void compilesBothBackendsWithoutLinking() {
		for (auto backend: { driver::BackendType::LLVM, driver::BackendType::DVM }) {
			auto script = fs::FileManager::createRandomVirtualFile(
				"fun saved_foo_name() -> i64 = { return 42; }\nvar x = saved_foo_name();"
			);
			global_state::setters::setScriptContext(script);
			driver::options_types::LinkingOptions linking_options;
			linking_options.no_link = true;
			ASSERT_TRUE(driver::compileScript(backend, {}, linking_options).isOk());
			const auto module_name = repl::getScriptModuleID(script).str();
			checkDumps(module_name);
			const auto object_suffix = backend == driver::BackendType::LLVM ? ".o" : ".o.dbc";
			ASSERT_TRUE(
				std::filesystem::file_size(artifacts_path.getPath() / (module_name + object_suffix))
				> 0
			);
			for (const auto* extension: { ".exe", ".dbc" })
				ASSERT_TRUE(!std::filesystem::exists(
					artifacts_path.getPath() / base::strConcat(script.stem(), extension)
				));
		}
	}

	void preservesDumpsAfterFailure() {
		auto script = fs::FileManager::createRandomVirtualFile(
			"fun saved_foo_name() -> i64 = { return 42; }\nlet x = saved_foo_name();\nlet z: i64 "
			"= \"abc\";"
		);
		global_state::setters::setScriptContext(script);
		driver::options_types::LinkingOptions linking_options;
		linking_options.no_link = true;
		ASSERT_TRUE(driver::compileScript(driver::BackendType::LLVM, {}, linking_options).isBad());
		const auto module_name = repl::getScriptModuleID(script).str();
		checkDumps(module_name);
		ASSERT_TRUE(!std::filesystem::exists(artifacts_path.getPath() / (module_name + ".o")));
	}

	void runOnDVMPanicShowsScriptLines() {
		// The functions are statements of their own, so their lines must be moved back from the
		// statement's file to the script's.
		auto script = fs::FileManager::createRandomVirtualFile(
			"fun divide(by: i64) -> i64 = {\n"
			"    return 10 / by;\n"
			"}\n"
			"\n"
			"fun forward(by: i64) -> i64 = {\n"
			"    return divide(by);\n"
			"}\n"
			"\n"
			"var x = forward(0);"
		);
		global_state::setters::setScriptContext(script);

		// The panic is reported on `std::cerr` by the execution thread, which the run joins.
		std::stringstream captured;
		auto*             original_cerr = std::cerr.rdbuf(captured.rdbuf());
		const auto        run           = driver::runScriptOnDVM(false);
		std::cerr.rdbuf(original_cerr);

		// A panic is an error of the run, which names it.
		ASSERT_TRUE(!run.has_value());
		assertTrue(run.error().contains("Panicked"), run.error());

		// The innermost frame says where its function is, the callers where their call is.
		const std::string stderr_output = captured.str();
		const std::string script_path   = script.getFilePath().string();
		assertTrue(
			stderr_output.contains(base::strConcat(
				"  #0 divide (defined at ",
				script_path,
				":1)\n",
				"  #1 forward at ",
				script_path,
				":6\n"
			)),
			"Each frame shows its Duckling name and script line: " + stderr_output
		);
		assertTrue(
			stderr_output.contains(base::strConcat(" at ", script_path, ":9\n")),
			"The script's own call is on its last line: " + stderr_output
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/");
