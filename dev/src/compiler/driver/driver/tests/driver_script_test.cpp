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
#include <iterator>
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
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/");
