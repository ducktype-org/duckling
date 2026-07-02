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
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/query_compiler/tests");
