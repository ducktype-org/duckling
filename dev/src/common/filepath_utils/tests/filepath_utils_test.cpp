#include <filepath_utils/file_uri.hpp>
#include <tester/tester.hpp>

#include <string>

class FilepathUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FilepathUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simplePathUri);
		TESTER_ADD_TEST(absolutePathUri);
		TESTER_ADD_TEST(emptyPathUri);
	}

private:
	void simplePathUri() {
		auto uri = filepath_utils::formatFileUri("some/path");
		assertTrue(uri == "file://some/path", "Relative path should use file:// scheme");
	}

	void absolutePathUri() {
		auto uri = filepath_utils::formatFileUri("/absolute/path");
		assertTrue(uri == "file:///absolute/path", "Absolute path should use file:/// with triple slash");
	}

	void emptyPathUri() {
		auto uri = filepath_utils::formatFileUri("");
		assertTrue(uri == "file://", "Empty path should produce bare file:// scheme");
	}
};

TESTER_COMMON_MAIN("/src/common/filepath_utils/tests/")
