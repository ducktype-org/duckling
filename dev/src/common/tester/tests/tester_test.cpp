// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/except/exceptions.hpp>

#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

#include <optional>
#include <variant>

class SimpleTesterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleTesterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(happy);
		TESTER_ADD_SHOULD_FAIL_TEST(failingIsNotThrowingStd);
		TESTER_ADD_SHOULD_FAIL_TEST(catchNoThrow);
		TESTER_ADD_SHOULD_FAIL_TEST(catchWrongThrow);
		TESTER_ADD_SHOULD_FAIL_TEST(assertTrueFails);
		TESTER_ADD_SHOULD_FAIL_TEST(assertFalseFails);
		TESTER_ADD_SHOULD_FAIL_TEST(assertEqualFails);
		TESTER_ADD_SHOULD_FAIL_TEST(throwPanic);
		TESTER_ADD_SHOULD_FAIL_TEST(throwLogicError);
		TESTER_ADD_SHOULD_FAIL_TEST(throwNYI);
		TESTER_ADD_SHOULD_FAIL_TEST(throwException);
		TESTER_ADD_SHOULD_FAIL_TEST(throwStdRuntimeError);
		TESTER_ADD_SHOULD_FAIL_TEST(throwStdLogicError);
		TESTER_ADD_TEST(assertMatchesPasses);
		TESTER_ADD_SHOULD_FAIL_TEST(assertMatchesFails);
		TESTER_ADD_SHOULD_FAIL_TEST(assertNotMatchesFails);
		TESTER_ADD_TEST(assertValuePasses);
		TESTER_ADD_SHOULD_FAIL_TEST(assertHasValueFails);
		TESTER_ADD_SHOULD_FAIL_TEST(assertNoValueFails);
		TESTER_ADD_TEST(verySimpleTestingUtilsTest);
		TESTER_ADD_TEST(addSpacesTest);
	}

	~SimpleTesterTest() override = default;

private:
	void throwPanic() { throw base::Panic("panic", "This is a panic test"); }

	void throwLogicError() { throw base::LogicError("Logic Error"); }

	void throwNYI() { throw base::NotYetImplemented("Not yet implemented"); }

	void throwException() { throw base::Exception(); }

	void throwStdRuntimeError() { throw std::runtime_error("This is a runtime error test"); }

	void throwStdLogicError() { throw std::logic_error("Logic Error"); }

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

	using TestVariant = std::variant<i32, char, bool>;

	void assertMatchesPasses() {
		TestVariant v = 'a';

		ASSERT_MATCHES(v, char);
		ASSERT_MATCHES(v, i32, char);
		ASSERT_NOT_MATCHES(v, i32);
		ASSERT_NOT_MATCHES(v, i32, bool);

		// The message variants: the message goes before the types.
		ASSERT_MATCHES_MSG(v, "The variant was assigned a char", char);
		ASSERT_MATCHES_MSG(v, "The variant was assigned a char", i32, char);
		ASSERT_NOT_MATCHES_MSG(v, "The variant was not assigned an i32", i32);
		ASSERT_NOT_MATCHES_MSG(v, base::strConcat("Nothing but a ", "char"), i32, bool);
	}

	void assertValuePasses() {
		std::optional<i32> some = 42;
		std::optional<i32> none;

		ASSERT_HAS_VALUE(some);
		ASSERT_NO_VALUE(none);

		// Everything after the operand is the message, concatenated like `strConcat` does it.
		ASSERT_HAS_VALUE(some, "The optional was assigned a value");
		ASSERT_NO_VALUE(none, "The optional was left empty");
		ASSERT_HAS_VALUE(some, "The optional was assigned ", 42);
	}

	void assertHasValueFails() {
		message("Expected to fail: checks that ASSERT_HAS_VALUE fails on an empty operand");
		std::optional<i32> none;
		ASSERT_HAS_VALUE(none, "expected failure: the optional is empty");
	}

	void assertNoValueFails() {
		message("Expected to fail: checks that ASSERT_NO_VALUE fails on a filled operand");
		std::optional<i32> some = 42;
		ASSERT_NO_VALUE(some, "expected failure: the optional holds a value");
	}

	void assertMatchesFails() {
		message("Expected to fail: checks that ASSERT_MATCHES fails on a different alternative");
		TestVariant v = 'a';
		ASSERT_MATCHES_MSG(v, "expected failure: the variant holds a char", i32, bool);
	}

	void assertNotMatchesFails() {
		message("Expected to fail: checks that ASSERT_NOT_MATCHES fails on a held alternative");
		TestVariant v = 'a';
		ASSERT_NOT_MATCHES(v, i32, char);
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

TESTER_COMMON_MAIN("/src/common/tester/tests/");
