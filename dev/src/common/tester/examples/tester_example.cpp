#include <tester/tester.hpp>

#include <thread>

// Class representing suite of tets
class MyTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MyTest

	// More code can be here

public:
	// more code can be here

	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// More code can be here

		TESTER_ADD_TEST(test1);

		// Tests can be repeated:
		TESTER_ADD_TEST(test1);

		TESTER_ADD_TEST(test2);
		TESTER_ADD_TEST(test3);
		TESTER_ADD_TEST(test4);
		TESTER_ADD_TEST(test5);
		TESTER_ADD_TEST(test6);
		TESTER_ADD_TEST(test7);
		TESTER_ADD_TEST(test8);
	}

private:
	// Write tests here:
	// Each test should be a method of signature void()
	// You can also write more methods or code here

	void test1() {}

	void test2() { fail("oops!"); }

	void test3() {
		assertTrue(true, "bad");
		assertTrue(false, "good");
	}

	void test4() {
		message("hi");
		fail("bye");
	}

	void test5() {
		assertTrue(false, "this is critical and stops the test");
		message("this is not displayed");
	}

	void test6() {
		assertTrue(false, "this is not critical and won't end the test", false);
		message("this is displayed");
	}

	void test7() {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(123ms);
	}

	void test8() { assertTrue(false, "oops!"); }
};

// If the main is same as below you can just write:
// TESTER_COMMON_MAIN("/common/tester/examples/");

int main() {
	// Relative path to test folder should be here:
	// It should in general be `/path-to-module/tests/`
	auto config = tester::getTestConfig("/common/tester/examples/");

	MyTest test(std::move(config));

	// Each test should return non-zero on exit to be noticed by ctest.
	// It will in the future be handled by tester module
	if (!test.run()) return 1;
}
