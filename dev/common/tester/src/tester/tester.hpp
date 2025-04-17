/**
 * @file tester.hpp
 * @author Andrzej
 *
 * @example tester_example.cpp
 */

#pragma once

#include "tester_config.hpp"

#include <init/init.hpp>  // IWYU pragma: export
#include <printer/stream_printer.hpp>

#include <base/exceptions.hpp>
#include <base/ints.hpp>

#include <exception>
#include <string>


#define ASSERT_EQUAL(expected, actual)                                                                 \
	assertEqual(                                                                                       \
		expected,                                                                                      \
		actual,                                                                                        \
		base::strConcat("Values not equal:\n\t\tIn line ", __LINE__, ": ", #expected, " != ", #actual) \
	)


#define ASSERT_TRUE(actual) ASSERT_EQUAL(true, actual)


class SimpleTesterTest;

namespace tester {

	/**
	 * @brief Add spaces before capital letters in a string,
	 * excluding: first letter, capital letters after capital letters.
	 *
	 * @todo improve the logic, so that "ABCSome" results in "ABC Some"
	 *
	 * @return std::string
	 */
	std::string addSpacesBeforeCapital(std::string_view);

	class TestSuite {
		friend SimpleTesterTest;

	protected:
		using TestType = void (TestSuite::*)();

	private:
		class CritTestError final: public std::exception {
		public:
			[[nodiscard]]
			const char* what() const noexcept final;
		};

		printer::StreamPrinter stream_printer;

		struct TestData final {
			TestType    test;
			std::string name;
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

		std::string           name;
		std::vector<TestData> tests;

	public:
		bool run();

		virtual ~TestSuite() = default;

	protected:
		TestConfig config;

		template<class T>
		auto path(const T& t) {
			return config.test_files_path + t;
		}

		TestSuite(TestConfig&& config, std::string_view name);
		void addTest(TestType test, std::string_view test_name);

		void assertTrue(bool v, std::string_view err, bool critical = true);
		void assertFalse(bool v, std::string_view err, bool critical = true);
		void fail(std::string_view err);
		void message(std::string_view mess);

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

#define TESTER_ADD_TEST(test) addTest(static_cast<TestType>(&TESTER_CLASS::test), #test)

#define TESTER_SUITE_NAME tester::addSpacesBeforeCapital(STRINGIFY_2(TESTER_CLASS))

#define TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                             \
	TESTER_CLASS(tester::TestConfig config __VA_OPT__(, ) __VA_ARGS__): \
		  tester::TestSuite(std::move(config), TESTER_SUITE_NAME)

/**
 * @brief Only use this macro if single class test file
 * and after defining proper TESTER_CLASS
 */
#define TESTER_COMMON_MAIN(test_path)                           \
	int main(int argc, const char* const*) {                    \
		init::InitObject _;                                     \
		if (argc != 1) CORE_PANIC("Test expects no arguments"); \
                                                                \
		auto config = tester::getTestConfig(test_path);         \
                                                                \
		TESTER_CLASS test(std::move(config));                   \
		if (!test.run()) return 1;                              \
	}

}
