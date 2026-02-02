
#include <base/except/exceptions.hpp>

#include <tester/tester.hpp>

#include <cstring>
#include <version>  // IWYU pragma: keep

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
		TESTER_ADD_TEST(testPanicStacktrace);
	}

	void throwPanic1() { throw base::Panic("POSITION: throwPanic1", "panic test"); }

	void throwPanic2() { CORE_PANIC("panic test 2"); }

	void throwPanic3() { CORE_ASSERT(false, "panic test 3"); }

	void throwPanic4() { CORE_UNREACHABLE(); }

	void testPanic1() {
		try {
			throwPanic1();
		} catch (base::Panic& panic) {
			assertTrue(containsCstr(panic.what(), "panic test"), "Bad panic reason");
			assertTrue(containsCstr(panic.what(), "POSITION: throwPanic1"), "Bad panic position");
			return;
		}
		fail("Panic what not caught");
	}

	void testPanic2() {
		try {
			throwPanic2();
		} catch (base::Panic& panic) {
			assertTrue(
				containsCstr(panic.what(), "    Panic thrown:\n    panic test 2"), "Bad panic reason"
			);
			return;
		}
		fail("Panic what not caught");
	}

	void testPanic3() {
		try {
			throwPanic3();
		} catch (base::Panic& panic) {
#if defined(BUILD_TYPE_DEV)
			assertTrue(
				containsCstr(panic.what(), "    Assertion failed: `false`\n    panic test 3"),
				"Bad panic reason"
			);
			return;
#else
			fail("Assert should now throw error in Release");
#endif
		}

#if defined(BUILD_TYPE_DEV)
		fail("Panic was not caught");
#endif
	}

	void testPanic4() {
		try {
			throwPanic4();
		} catch (base::Panic& panic) {
			assertTrue(containsCstr(panic.what(), "Unreachable"), "Bad panic reason");
			return;
		}
#if defined(BUILD_TYPE_DEV)
		fail("Panic was not caught.");
#endif
	}

	void testNotYetImplemented() {
		try {
			throw base::NotYetImplemented("NotYetImplemented test");
		} catch (base::NotYetImplemented& nyi) {
			assertTrue(
				compareCstr(
					nyi.what(), "The feature is not implemented yet: NotYetImplemented test"
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

	void testPanicStacktrace() {
		try {
			throwPanic1();
		} catch (base::Panic& panic) {
			const char* what_str = panic.what();
			std::cerr << "Panic what: " << what_str << "\n";
#ifdef __cpp_lib_stacktrace
			// Check for stacktrace fragments (e.g. "0#", "1#", "2#").
			// These are typical markers in the string output of std::stacktrace,
			// so their presence indicates that a stacktrace was actually generated.
			bool found0 = std::strstr(what_str, "0#") != nullptr;
			bool found1 = std::strstr(what_str, "1#") != nullptr;
			bool found2 = std::strstr(what_str, "2#") != nullptr;
			assertTrue(found0 || found1 || found2, "Stacktrace not found in Panic what()");
#else
			// Check that the string is not empty and contains the fallback message.
			// If stacktrace is not supported, we expect a specific message to be present.
			assertTrue(std::strlen(what_str) > 0, "Panic what() is empty");
			assertTrue(
				std::strstr(what_str, "Stack trace is not supported") != nullptr,
				"Missing fallback stacktrace message"
			);
#endif
			return;
		}
		fail("Panic what not caught");
	}

	~ExceptionTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");
