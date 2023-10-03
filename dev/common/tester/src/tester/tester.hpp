/**
 * @file tester.hpp
 * @author Andrzej
 */

#pragma once

#include "tester_config.hpp"

#include <config/config.hpp>
#include <exception>
#include <printer/printer.hpp>
#include <string>

class SimpleTesterTest;

namespace tester {

	class TestSuite {
		friend SimpleTesterTest;

	protected:
		typedef void (TestSuite::*TestType)();

	private:
		class CritTestError: public std::exception {
		public:
			const char* what() const noexcept final;
		};

		printer::Console console;

		struct TestData {
			TestType    test;
			std::string name;
		};

		struct TestResult {
			bool                 success = true;
			bool                 stop    = false;
			printer::MessagePack output;
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

	protected:
		TestConfig config;

		template<class T>
		auto path(T&& t) {
			return config.test_files_path + t;
		}

		TestSuite(TestConfig&& config, std::string_view name);
		virtual ~TestSuite() = default;
		void addTest(TestType test, std::string_view test_name);

		void assert(bool v, std::string_view err, bool critical = true);
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
	};

#define TESTER_CLASS TESTER_CLASS_MUST_BE_DEFINED_BEFORE_ANY_TEST_CLASS

#define TESTER_ADD_TEST(test) addTest(static_cast<TestType>(&TESTER_CLASS::test), #test)

#define TESTER_TEST_SIMPLE_CONSTRUCTOR(name, ...)                         \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  tester::TestSuite(std::move(config), name)

/**
 * @brief Only use this macro if single class test file
 * and after defining proper TESTER_CLASS
 */
#define TESTER_COMMON_MAIN(test_path)                                        \
	int main(int argc, char* argv[]) {                                       \
		auto config = tester::testConfigFromArgs({ argc, argv }, test_path); \
                                                                             \
		TESTER_CLASS test(std::move(config));                                \
		if (!test.run()) return 1;                                           \
	}

}
