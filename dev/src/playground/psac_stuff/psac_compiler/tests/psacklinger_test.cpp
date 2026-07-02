#include <compiler_test_corpus/compiler_test_corpus.hpp>
#include <psackling/compiler.hpp>

#include <filesystem/file.hpp>
#include <system_command/system_command.hpp>
#include <tester/tester.hpp>

class PsacklingerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PsacklingerTest

	usize single_file_id = 0;
	usize multi_file_id  = 0;

	void singleFileCorpusTest() {
		const psack_corpus::SingleFileCase& data
			= psack_corpus::singleFileCases()[single_file_id++];
		std::cout << "Running test: " << data.name << '\n';
		fs::File f                 = fs::FileManager::createRandomVirtualFile(data.code);
		fs::File random_output_dir = fs::FileManager::createRandomTempDirectory();
		fs::File executable        = random_output_dir.createSubFile("", "psacklinged.out");
		pg::compile(f, executable.getFilePath().string());
		ASSERT_EQUAL_PRINT(data.expected_exit_code, runExecutable(executable));
	}

	void multiFileCorpusTest() {
		const psack_corpus::MultiFileCase& data = psack_corpus::multiFileCases()[multi_file_id++];
		std::cout << "Running test: " << data.name << '\n';
		fs::File root = psack_corpus::makeProgramDir(data.files);

		if (data.error_substring == nullptr) {
			ASSERT_EQUAL_PRINT(data.expected_exit_code, compileAndRun(root));
			return;
		}
		try {
			compileAndRun(root);
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) {
			ASSERT_TRUE(std::string(e.what()).contains(data.error_substring));
		}
	}

	static u64 runExecutable(const fs::File& executable) {
		system_command::SystemCommand cmd(executable.getFilePath().string());
		return u64(cmd.execute(system_command::SystemCommand::ExitCodeHandling::Ignore));
	}

	static u64 compileAndRun(const fs::File& root) {
		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "multifile.out");
		pg::compile(root, executable.getFilePath().string());
		return runExecutable(executable);
	}

	/// Changing one function body in an imported file recompiles exactly that function.
	void incrementalImportedBodyChange() {
		const std::string root_code
			= "import math as M;\n"
			  "fun main() -> i64 = { return M.helper() + 1; }\n";

		fs::File           root = psack_corpus::makeProgramDir({
            { "root.dmf", root_code.c_str() },
            { "math.dmf", "fun helper() -> i64 = { return 10; }\n" },
        });
		const fs::FilePath math_path{ root.getFilePath().parentPath().getPath() / "math.dmf" };

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_import.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		pg::CompileStats initial = compiler.compile(root);
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		pg::CompileStats unchanged = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		fs::FileManager::createVirtualFile(
			math_path, "fun helper() -> i64 = { return 20; }\n", /*allow_overwrite=*/true
		);
		pg::CompileStats incremental = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a const in an imported file recompiles everything (coarse globals hash).
	void incrementalImportedConstChange() {
		fs::File           root = psack_corpus::makeProgramDir({
            { "root.dmf",
		                "import conf.*;\n"
		                          "fun main() -> i64 = { return X + 1; }\n" },
            { "conf.dmf", "const X: i64 = 5;\n" },
        });
		const fs::FilePath conf_path{ root.getFilePath().parentPath().getPath() / "conf.dmf" };

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_import_const.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(root);
		ASSERT_EQUAL_PRINT(u64(6), runExecutable(executable));

		fs::FileManager::createVirtualFile(
			conf_path, "const X: i64 = 8;\n", /*allow_overwrite=*/true
		);
		pg::CompileStats incremental = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Changing one function body recompiles exactly that function; identical source
	/// recompiles nothing.
	void incrementalBodyChange() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper() + 1; }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 20; }
			fun main() -> i64 = { return helper() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		pg::CompileStats initial = compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), initial.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		pg::CompileStats unchanged = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a global constant recompiles every function (coarse globals dependency).
	void incrementalGlobalsChange() {
		const std::string v1 = R"(
			const X: i64 = 5;
			fun helper() -> i64 = { return X; }
			fun main() -> i64 = { return helper() + X; }
		)";
		const std::string v2 = R"(
			const X: i64 = 7;
			fun helper() -> i64 = { return X; }
			fun main() -> i64 = { return helper() + X; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_globals.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(14), runExecutable(executable));
	}

	/// Changing a global variable initializer re-emits the globals object and (via the
	/// coarse globals hash) recompiles every function.
	void incrementalGlobalVarChange() {
		const std::string v1 = R"(
			var g: i64 = 5;
			fun main() -> i64 = { return g; }
		)";
		const std::string v2 = R"(
			var g: i64 = 9;
			fun main() -> i64 = { return g; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_gvar.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(5), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Adding a function changes the function set, which falls back to a full rebuild.
	void incrementalFunctionSetChange() {
		const std::string v1 = R"(
			fun main() -> i64 = { return 1; }
		)";
		const std::string v2 = R"(
			fun extra() -> i64 = { return 41; }
			fun main() -> i64 = { return extra() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_set.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(1), runExecutable(executable));

		pg::CompileStats rebuilt = compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), rebuilt.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), rebuilt.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(42), runExecutable(executable));
	}

	/// Incremental updates also work in --sequential mode (hash diff without PSAC).
	void incrementalSequentialMode() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 30; }
			fun main() -> i64 = { return helper(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_seq.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string(),
		                                          .sequential  = true });

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(30), runExecutable(executable));
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		for (const psack_corpus::SingleFileCase& corpus_case: psack_corpus::singleFileCases()) {
			addTest(
				static_cast<TestType>(&PsacklingerTest::singleFileCorpusTest),
				corpus_case.name,
				false
			);
		}
		for (const psack_corpus::MultiFileCase& corpus_case: psack_corpus::multiFileCases()) {
			addTest(
				static_cast<TestType>(&PsacklingerTest::multiFileCorpusTest), corpus_case.name, false
			);
		}

		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalBodyChange),
			"IncrementalBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalGlobalsChange),
			"IncrementalGlobalsChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalGlobalVarChange),
			"IncrementalGlobalVarChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalFunctionSetChange),
			"IncrementalFunctionSetChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalSequentialMode),
			"IncrementalSequentialMode",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalImportedBodyChange),
			"IncrementalImportedBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalImportedConstChange),
			"IncrementalImportedConstChange",
			false
		);
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/psac_compiler/tests");
