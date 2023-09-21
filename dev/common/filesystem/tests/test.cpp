#include <filesystem/file.hpp>
#include <tester/tester.hpp>

class SimpleFileSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleFileSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple FileSystem Test") {
		TESTER_ADD_TEST(getSimpleContentTest);
		TESTER_ADD_TEST(filePathTest);
	}

private:

	void getSimpleContentTest() {
		auto a_content = fs::getSimpleFileContent(path("a_file.txt"));

		assert(a_content.view().size() == 12, "Wrong a_content file size");
		assert(a_content.view().stringView() == "abrakadabra\n", "Wrong a_content file content");
	}

	void filePathTest() {
		auto a_good_content = fs::getSimpleFileContent(path("a_file.txt"));
		auto b_good_content = fs::getSimpleFileContent(path("b_file.txt"));

		// loops twice to see if behavior is ok after all previous fileContents where destroyed
		for (i32 i = 0; i < 2; i++) {
			fs::FilePath a1(path("a_file.txt"));
			fs::FilePath a2(path("a_file.txt"));
			fs::FilePath b1(path("b_file.txt"));

			auto a1_content = a1.getContent();
			auto a2_content = a1.getContent();
			auto b1_content = b1.getContent();

			assert(a1_content.view().getBegin() == a1.getContent().view().getBegin(),
			       "a_file was read multiple times when it shouldn't");
			assert(a2_content.view().getBegin() == a1.getContent().view().getBegin(),
			       "a_file was read multiple times when it shouldn't");
			assert(a1_content.view().getBegin() == a2_content.view().getBegin(),
			       "a_file was read multiple times when it shouldn't");

			assert(a1_content.view().stringView() == a_good_content.view().stringView(), "Wrong a_file content");
			assert(b1_content.view().stringView() == b_good_content.view().stringView(), "Wrong b_file content");
		}
	}
};

TESTER_COMMON_MAIN("/common/filesystem/tests/");
