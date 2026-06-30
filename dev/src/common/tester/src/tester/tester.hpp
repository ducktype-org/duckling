/**
 * @file tester.hpp
 * @author Andrzej
 *
 * @example tester_example.cpp
 */

#pragma once

#include "tester_config.hpp"

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <init/init.hpp>  // IWYU pragma: export
#include <printer/stream_printer.hpp>

#include <exception>
#include <string>


#define ASSERT_EQUAL(expected, actual)                                                    \
	assertEqual(                                                                          \
		expected,                                                                         \
		actual,                                                                           \
		base::strConcat(                                                                  \
			"Values not equal:\n\t\tIn line ", __LINE__, ": ", #expected, " != ", #actual \
		)                                                                                 \
	)

#define ASSERT_EQUAL_PRINT(expected, actual)               \
	assertEqual(                                           \
		expected,                                          \
		actual,                                            \
		base::strConcat(                                   \
			"Values not equal:\n\t\tIn line ",             \
			__LINE__,                                      \
			":\n\t\t\t",                                   \
			#expected,                                     \
			" != ",                                        \
			#actual,                                       \
			"\n\t\t\t",                                    \
			base::escapeString(base::strConcat(expected)), \
			" != ",                                        \
			base::escapeString(base::strConcat(actual))    \
		)                                                  \
	)


#define ASSERT_TRUE(actual) ASSERT_EQUAL(true, actual)

#define ASSERT_HAS_VALUE(actual) ASSERT_EQUAL(true, tester::detail::hasValue(actual))

#define ASSERT_NO_VALUE(actual) ASSERT_EQUAL(false, tester::detail::hasValue(actual))


class SimpleTesterTest;

namespace tester {

	namespace detail {
		template<typename>
		inline constexpr bool ALWAYS_FALSE = false;

		/**
		 * @brief Resolves hasValue() for multiple cases. Works both on direct types and pointers as
		 * well as `has_value()` and `hasValue()`
		 */
		template<typename T>
		bool hasValue(const T& value) {
			if constexpr (requires { value.has_value(); }) {
				return value.has_value();
			} else if constexpr (requires { value.hasValue(); }) {
				return value.hasValue();
			} else if constexpr (requires { value->has_value(); }) {
				return value->has_value();
			} else if constexpr (requires { value->hasValue(); }) {
				return value->hasValue();
			} else {
				static_assert(
					ALWAYS_FALSE<T>,
					"ASSERT_HAS_VALUE requires an operand with .has_value(), .hasValue(), "
					"->has_value() or ->hasValue()"
				);
				return false;
			}
		}
	}

	/**
	 * @brief Add spaces before capital letters in a string,
	 * excluding: first letter, capital letters after capital letters.
	 *
	 * @return std::string
	 */
	std::string addSpacesBeforeCapital(std::string_view);

	class TestSuite {
		friend SimpleTesterTest;

	protected:
		using TestType = void (TestSuite::*)();

		/**
		 * Method called once just before running the first test in a suite.
		 */
		virtual void beforeAll() {}

		/**
		 * Method called once just after running the last test in a suite.
		 */
		virtual void afterAll() {}

	private:
		class CritTestError final: public std::exception {
		public:
			CritTestError(std::string_view message = "Critical test failure.");

			[[nodiscard]]
			const char* what() const noexcept final;
		};

		printer::StreamPrinter stream_printer;

		struct TestData final {
			TestType    test;
			std::string name;
			bool        should_fail = false;
		};

		struct TestResult final {
			bool                        success = true;
			bool                        stop    = false;
			printer::PrinterContentsSeq output;
		};

		void resultHandler(const TestData& test, const TestResult& res);
		void prolog();
		void epilog(usize passed, usize failed, double time);

		TestResult* curr_global_res;
		void        runTest(TestType test);

		std::string              name;
		std::vector<TestData>    tests;
		std::vector<std::string> failed_tests;

	public:
		/**
		 * @brief Runs all the tests in the test suite. Prints tests statistics and list of failed
		 * tests in case of failure.
		 * @return True if all tests passed, false otherwise
		 */
		bool run();

		/**
		 * @brief Filters the tests in the TestSuite. Removes all tests from the test suite which
		 * don't appear in `tests_to_run` vector. Prints warning messages when `tests_to_run`
		 * contains a test name which doesn't exist in the test suite.
		 */
		void filterTests(const std::vector<std::string>& tests_to_run);

		virtual ~TestSuite() = default;

	protected:
		TestConfig config;

		template<class T>
		auto path(const T& t) {
			return config.test_files_path + t;
		}

		TestSuite(TestConfig&& config, std::string_view name);
		void addTest(TestType test, std::string_view test_name, bool should_fail);

		void         assertTrue(bool v, std::string_view err, bool critical = true);
		void         assertFalse(bool v, std::string_view err, bool critical = true);
		virtual void fail(std::string_view err, bool critical = true);
		void         message(std::string_view mess);

		template<typename Exception, typename FuncType>
		void assertThrows(const FuncType& func, std::string_view error) {
			try {
				func();
			} catch (const CritTestError& e) {
				fail("CritTestError thrown in assertThrows");
			} catch (const Exception& e) { return; }
			fail(error);
		}

		// rvalue reference to make sure the order is correct for the macro above.
		template<class T, class U>
		void assertEqual(const T& expected, const U& actual, std::string_view error) {
			assertTrue(expected == actual, error);
		}
	};

#define TESTER_CLASS TESTER_CLASS_MUST_BE_DEFINED_BEFORE_ANY_TEST_CLASS

#define TESTER_ADD_TEST(test) addTest(static_cast<TestType>(&TESTER_CLASS::test), #test, false)

#define TESTER_ADD_SHOULD_FAIL_TEST(test) \
	addTest(static_cast<TestType>(&TESTER_CLASS::test), #test, true)

#define TESTER_SUITE_NAME tester::addSpacesBeforeCapital(STRINGIFY_2(TESTER_CLASS))

#define TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                             \
	TESTER_CLASS(tester::TestConfig config __VA_OPT__(, ) __VA_ARGS__): \
		  tester::TestSuite(std::move(config), TESTER_SUITE_NAME)

/**
 * @brief Only use this macro if single class test file
 * and after defining proper TESTER_CLASS
 */
#define TESTER_COMMON_MAIN(test_path)                                          \
	int main(int argc, const char* const* argv) {                              \
		init::InitObject _;                                                    \
		auto             config = tester::getTestConfig(test_path);            \
                                                                               \
		TESTER_CLASS test(std::move(config));                                  \
                                                                               \
		std::vector<std::string> tests_to_run;                                 \
		for (int i = 1; i < argc; i++) { tests_to_run.emplace_back(argv[i]); } \
		test.filterTests(tests_to_run);                                        \
		if (!test.run()) return 1;                                             \
	}

}
