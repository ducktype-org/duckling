#include <filepath_utils/file_uri.hpp>
#include <tester/tester.hpp>

#include <string>

// NOTE: The Windows drive-letter path (file:///C:/path) is not tested here
// because this test runs on Linux where _WIN32 is not defined. The Windows
// branch in formatFileUri is a one-line #ifdef that prepends an extra '/'
// before the drive letter. This is an accepted, documented blind spot: the
// contract is absolute paths only (see file_uri.hpp), and the planned
// coverage is a Windows CI runner (@TODO: #3343 Add Windows CI coverage for
// os_utils/filepath_utils platform branches); until then, review of
// file_uri.cpp is the only check for that branch.

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
		assertTrue(
			uri == "file:///absolute/path", "Absolute path should use file:/// with triple slash"
		);
	}

	void emptyPathUri() {
		auto uri = filepath_utils::formatFileUri("");
		assertTrue(uri == "file://", "Empty path should produce bare file:// scheme");
	}
};

TESTER_COMMON_MAIN("/src/common/filepath_utils/tests/")
