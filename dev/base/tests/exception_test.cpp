#include <tester/tester.hpp>
#include <filesystem/file.hpp>
#include <base/str_utils.hpp>
#include <cstring>

bool compareCstr(const char* const c1, const char* const c2) {
	return std::string_view(c1) == std::string_view(c2);
}

bool containsCstr(const char* const base, const char* const pattern) {
	return std::strstr(base, pattern) != nullptr;
}

class ExceptionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ExceptionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testPanic1);
		TESTER_ADD_TEST(testPanic2);
		TESTER_ADD_TEST(testPanic3);
		TESTER_ADD_TEST(testNotYetImplemented);
		TESTER_ADD_TEST(testLogicError);
	}

	void throwPanic1() { throw base::Panic("throwPanic", "panic test"); }

	void throwPanic2() { CORE_PANIC("panic test 2"); }

	void throwPanic3() { CORE_ASSERT(false, "panic test 3"); }

	void testPanic1() {
		try {
			throwPanic1();
		} catch (base::Panic& panic) {
			assertTrue(panic.getPosition() == "throwPanic", "Bad panic position");
			assertTrue(containsCstr(panic.what(), "panic test"), "Bad panic reason");
			return;
		}
		fail("Panic what not caught");
	}

	void testPanic2() {
		try {
			throwPanic2();
		} catch (base::Panic& panic) {
			assertTrue(
				containsCstr(panic.what(), "    Panic thrown:\n    panic test 2"),
				"Bad panic reason"
			);
			return;
		}
		fail("Panic what not caught");
	}

	void testPanic3() {
		try {
			throwPanic3();
		} catch (base::Panic& panic) {
			assertTrue(
				containsCstr(panic.what(), "    Assertion failed: `false`\n    panic test 3"),
				"Bad panic reason"
			);
			return;
		}
		fail("Panic what not caught");
	}

	void testNotYetImplemented() {
		try {
			throw base::NotYetImplemented("NotYetImplemented test");
		} catch (base::NotYetImplemented& nyi) {
			assertTrue(
				compareCstr(
					nyi.what(), "The feature is not implemented yet.\nNotYetImplemented test"
				),
				"Bad NotYetImplemented reason"
			);
			return;
		}
		fail("NotYetImplemented what not caught");
	}

	void testLogicError() {
		try {
			throw base::LogicError("Logic error test");
		} catch (base::LogicError& le) {
			assertTrue(compareCstr(le.what(), "Logic error test"), "Bad LogicError reason");
			return;
		}
		fail("LogicError what not caught");
	}

	~ExceptionTest() override = default;

private:
};

TESTER_COMMON_MAIN("/base/tests/");
