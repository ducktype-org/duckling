#include <compiler_test_corpus/compiler_test_corpus.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <querylinger/queries/version.hpp>
#include <querylinger/session.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <system_command/system_command.hpp>
#include <tester/tester.hpp>

class QuerylingerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QuerylingerTest

	usize single_file_id = 0;
	usize multi_file_id  = 0;

	void versionQueryComputesOnce() {
		const std::string first = query::entryPoint<ql::QueryCompilerVersion>(ql::VersionKey{});
		const usize       calls_after_first = ql::versionProvideCalls();

		const std::string second = query::entryPoint<ql::QueryCompilerVersion>(ql::VersionKey{});

		ASSERT_EQUAL_PRINT(std::string(ql::QUERYLINGER_VERSION), first);
		ASSERT_EQUAL_PRINT(first, second);
		ASSERT_EQUAL_PRINT(usize(1), calls_after_first);
		// The second entry point call must be served from the cache.
		ASSERT_EQUAL_PRINT(calls_after_first, ql::versionProvideCalls());
	}

	void singleFileCorpusTest() {
		const psack_corpus::SingleFileCase& data
			= psack_corpus::singleFileCases()[single_file_id++];
		std::cout << "Running test: " << data.name << '\n';
		fs::File f                 = fs::FileManager::createRandomVirtualFile(data.code);
		fs::File random_output_dir = fs::FileManager::createRandomTempDirectory();
		fs::File executable        = random_output_dir.createSubFile("", "querylinged.out");
		ql::compile(f, executable.getFilePath().string());
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

	/// Changing one function body recompiles exactly that function; identical source
	/// recompiles nothing (and skips the relink).
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

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		ql::CompileStats initial = compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), initial.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		ql::CompileStats unchanged = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		ql::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a global constant recompiles every function — parity with
	/// psacklinger's coarse globals dependency (made fine-grained in later
	/// milestones).
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

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		ql::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(14), runExecutable(executable));
	}

	/// Changing a global variable initializer re-emits the globals object and (via
	/// the coarse globals input) recompiles every function.
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

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(5), runExecutable(executable));

		ql::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Adding a function creates a new query node without psacklinger's full-rebuild
	/// fallback: only the new function and the changed main are compiled.
	void incrementalFunctionAdded() {
		const std::string v1 = R"(
			fun main() -> i64 = { return 1; }
		)";
		const std::string v2 = R"(
			fun extra() -> i64 = { return 41; }
			fun main() -> i64 = { return extra() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_set.out");

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(1), runExecutable(executable));

		ql::CompileStats grown = compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), grown.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), grown.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(42), runExecutable(executable));
	}

	/// Removing a function drops its object from the link without psacklinger's
	/// full-rebuild fallback. The survivors still recompile because the removed
	/// signature is part of the coarse globals input (fine-grained signature inputs
	/// land in a later milestone).
	void incrementalFunctionRemoved() {
		const std::string v1 = R"(
			fun extra() -> i64 = { return 41; }
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_removed.out");

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		ql::CompileStats shrunk = compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), shrunk.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), shrunk.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));
	}

	/// Changing one function body in an imported file recompiles exactly that
	/// function.
	void incrementalImportedBodyChange() {
		fs::File           root = psack_corpus::makeProgramDir({
            { "root.dmf",
		                "import math as M;\n"
		                          "fun main() -> i64 = { return M.helper() + 1; }\n" },
            { "math.dmf", "fun helper() -> i64 = { return 10; }\n" },
        });
		const fs::FilePath math_path{ root.getFilePath().parentPath().getPath() / "math.dmf" };

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_import.out");

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		ql::CompileStats initial = compiler.compile(root);
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		ql::CompileStats unchanged = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		fs::FileManager::createVirtualFile(
			math_path, "fun helper() -> i64 = { return 20; }\n", /*allow_overwrite=*/true
		);
		ql::CompileStats incremental = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a const in an imported file recompiles every function (coarse
	/// globals input — one function total here, so 1/1).
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

		ql::Compiler compiler(ql::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(root);
		ASSERT_EQUAL_PRINT(u64(6), runExecutable(executable));

		fs::FileManager::createVirtualFile(
			conf_path, "const X: i64 = 8;\n", /*allow_overwrite=*/true
		);
		ql::CompileStats incremental = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	static u64 compileAndRun(const fs::File& root) {
		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "multifile.out");
		ql::compile(root, executable.getFilePath().string());
		return runExecutable(executable);
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		concurrent::worker::setWorkerCount(4);
		query::setTrackReverseGraph(true);

		TESTER_ADD_TEST(versionQueryComputesOnce);

		for (const psack_corpus::SingleFileCase& corpus_case: psack_corpus::singleFileCases()) {
			addTest(
				static_cast<TestType>(&QuerylingerTest::singleFileCorpusTest),
				corpus_case.name,
				false
			);
		}
		for (const psack_corpus::MultiFileCase& corpus_case: psack_corpus::multiFileCases()) {
			addTest(
				static_cast<TestType>(&QuerylingerTest::multiFileCorpusTest), corpus_case.name, false
			);
		}

		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalBodyChange),
			"IncrementalBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalGlobalsChange),
			"IncrementalGlobalsChange",
			false
		);
		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalGlobalVarChange),
			"IncrementalGlobalVarChange",
			false
		);
		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalFunctionAdded),
			"IncrementalFunctionAdded",
			false
		);
		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalFunctionRemoved),
			"IncrementalFunctionRemoved",
			false
		);
		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalImportedBodyChange),
			"IncrementalImportedBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&QuerylingerTest::incrementalImportedConstChange),
			"IncrementalImportedConstChange",
			false
		);
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/query_compiler/tests");
