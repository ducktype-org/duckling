#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

#include <base/exceptions.hpp>

class SimpleTesterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleTesterTest
	i32 test_no;
	i32 exception_no = 0;

public:
	static constexpr i32 NUM_FAILING_TESTS   = 7;
	static constexpr i32 NUM_EXCEPTION_TESTS = 7;

	TESTER_TEST_SIMPLE_CONSTRUCTOR(i32 test_no, i32 exception_no), test_no(test_no),
		exception_no(exception_no) {
		TESTER_ADD_TEST(choose);
		TESTER_ADD_TEST(throwException);
		TESTER_ADD_TEST(verySimpleTestingUtilsTest);
		TESTER_ADD_TEST(addSpacesTest);
	}

	~SimpleTesterTest() override = default;

private:
	void choose() {
		switch (test_no) {
		case 0:
			return happy();
		case 1:
			return failingIsNotThrowingStd();
		case 2:
			return catchNoThrow();
		case 3:
			return failingIsNotThrowingStd();
		case 4:
			return assertTrueFails();
		case 5:
			return assertFalseFails();
		case 6:
			return assertEqualFails();
		default:
			return;
		}
	}

	void throwException() {
		switch (exception_no) {
		case 0:
			return;
		case 1:
			throw base::Panic("panic", "This is a panic test");
		case 2:
			throw base::LogicError("Logic Error");
		case 3:
			throw base::NotYetImplemented("Not yet implemented");
		case 4:
			throw base::Exception();
		case 5:
			throw std::runtime_error("This is a runtime error test");
		case 6:
			throw std::logic_error("Logic Error");
		default:
			return;
		}
	}

	void happy() {
		assertThrows<std::logic_error>([&]() { throw std::logic_error("Hi!"); }, "Logic Error");

		assertThrows<std::exception>([&]() { throw std::exception(); }, "std::exception");

		assertThrows<i32>([&]() { throw 3; }, "int");
	}

	void failingIsNotThrowingStd() {
		// Expected to fail: checks that failing a test is not mistaken for throwing std::exception.
		try {
			assertThrows<std::exception>(
				[&]() { fail("Please wait patiently for the failure of the system..."); },
				"This message is unfortunately discarded."
			);
		} catch (const tester::TestSuite::CritTestError& e) {
			message("Task failed successfully.");
		}
	}

	void catchNoThrow() {
		message("Expected to fail: checks that assertThrows fails when no exception is caught");
		assertThrows<std::logic_error>([&]() {}, "expected failure: No exception was thrown");
	}

	void catchWrongThrow() {
		message("Expected to fail: checks that assertThrows fails when wrong exception is caught.");
		assertThrows<std::logic_error>(
			[&]() { throw std::exception(); }, "expected failure: Wrong exception was thrown"
		);
	}

	void assertTrueFails() {
		message("Expected to fail: checks that assertTrue fails when condition is false");
		assertTrue(false, "expected failure: assertTrue failed");
	}

	void assertFalseFails() {
		message("Expected to fail: checks that assertFalse fails when condition is true");
		assertFalse(true, "expected failure: assertFalse failed");
	}

	void assertEqualFails() {
		message("Expected to fail: checks that assertEqual fails when values are not equal");
		assertEqual(1, 2, "expected failure: assertEqual failed");
	}

	void verySimpleTestingUtilsTest() {
		assertTrue(testing_utils::compareJson(" {}", "{ }"), "Incorrect compareJson (1)");

		assertTrue(
			testing_utils::compareJson(R"--( { "data" : {} })--", R"--(  { "data" : {  } } )--"),
			"Incorrect compareJson (2)"
		);

		assertTrue(
			!testing_utils::compareJson(R"--( { "data" : [] })--", R"--(  { "data" : {  } } )--"),
			"Incorrect compareJson (3)"
		);

		assertTrue(
			!testing_utils::compareJson(
				R"--( { "data" :  { }, "data2" : {} })--", R"--(  { "data" : {  } } )--"
			),
			"Incorrect compareJson (4)"
		);
	}

	void addSpacesTest() {
		assertEqual(
			tester::addSpacesBeforeCapital("HelloWorld"),
			"Hello World",
			"Incorrect addSpacesBeforeCapital (1)"
		);
		assertEqual(
			tester::addSpacesBeforeCapital("HelloWorldTest"),
			"Hello World Test",
			"Incorrect addSpacesBeforeCapital (2)"
		);
		assertEqual(
			tester::addSpacesBeforeCapital("HelloWorldTest123"),
			"Hello World Test123",
			"Incorrect addSpacesBeforeCapital (3)"
		);
		assertEqual(
			tester::addSpacesBeforeCapital("HelloWorldABC"),
			"Hello World ABC",
			"Incorrect addSpacesBeforeCapital (4)"
		);
		assertEqual(
			tester::addSpacesBeforeCapital("ABCHelloWorld"),
			"ABC Hello World",
			"Incorrect addSpacesBeforeCapital (5)"
		);
		assertEqual(
			tester::addSpacesBeforeCapital("ABC hello world"),
			"ABC hello world",
			"Incorrect addSpacesBeforeCapital (6)"
		);
		assertEqual(
			tester::addSpacesBeforeCapital("hello world ABC"),
			"hello world ABC",
			"Incorrect addSpacesBeforeCapital (7)"
		);
	}
};

int main(int argc, const char**) {
	if (argc != 1) CORE_PANIC("Test expects no arguments");

	auto config = tester::getTestConfig("/common/tester/tests/");

	SimpleTesterTest passing_test(config, 0, 0);
	if (!passing_test.run()) return 1;

	/* The tests below are "expected to fail".
	 * Currently there is no way to specify that.
	 * @TODO: change that when "expected to fail" is added #1049
	 */
	for (i32 i = 1; i < SimpleTesterTest::NUM_FAILING_TESTS; i++) {
		SimpleTesterTest failing_test(config, i, 0);
		if (failing_test.run()) return 1;
	}
	for (i32 i = 1; i < SimpleTesterTest::NUM_EXCEPTION_TESTS; i++) {
		SimpleTesterTest exception_test(config, 0, i);
		if (exception_test.run()) return 1;
	}
}
