// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <global_state/artifacts_location.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <fstream>

using namespace compiler;

/**
 * The compiler can be initialized once per process, so this suite has a binary of its own.
 */
class DriverArtifactsFolderTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverArtifactsFolderTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(folderThatCannotBeCreatedFailsCleanly); }

private:
	// A folder that cannot be created - here, because a file is in the way - is the
	// environment's fault, not a compiler bug: initialization fails with a reason instead of
	// panicking, and nothing is left to flush at exit.
	void folderThatCannotBeCreatedFailsCleanly() {
		const auto dir  = fs::FileManager::createRandomTempDirectory().getFilePath().getPath();
		const auto file = dir / "a_file";
		std::ofstream(file) << "in the way";

		auto initialized
			= driver::initializeTheCompiler(driver::CompilerModeOfOperationAndOptions::ScriptMode{
				.script_file     = fs::FileManager::createRandomVirtualFile(""),
				.backend_options = { .llvm_backend = global_state::BackendOptions::LLVMBackend{} },
				.compilation_artifacts = { .artifacts_path = fs::FilePath(file / "artifacts") },
				.debug_options         = {},
				.execution_options     = { .worker_count = 1 },
				.stdlib_options        = {},
			});
		ASSERT_TRUE(initialized.status().isBad());
		ASSERT_TRUE(!global_state::hasRootCollection());
		driver::exit();
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/");
