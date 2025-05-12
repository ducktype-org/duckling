#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>
#include <typesystem/higher/all.hpp>

using namespace tsh;

class HigherTypeSystemErrorTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HigherTypeSystemErrorTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(integralSizeErrorTest);
		TESTER_ADD_TEST(floatSizeErrorTest);
	}

private:
	void integralSizeErrorTest() {
		query::Context::logger.clear();
		assertTrue(query::Context::logger.good(), "Test should begin without errors.");
		query::entryPoint<QueryIntegralType>({ 42 });

		std::stringstream dumped_logs;
		assertTrue(
			query::Context::logger.bad(), "Requesting bad integral size should result in an error."
		);
		query::Context::logger.dumpLog(false, dumped_logs);
		const auto dumped_logs_str = dumped_logs.str();
		assertTrue(
			dumped_logs_str.find("Invalid size of integral type") != decltype(dumped_logs_str)::npos,
			"Logs should contain mention of invalid integral size."
		);
	}

	void floatSizeErrorTest() {
		query::Context::logger.clear();
		assertTrue(query::Context::logger.good(), "Test should begin without errors.");
		query::entryPoint<QueryFloatType>(42);

		std::stringstream dumped_logs;
		assertTrue(
			query::Context::logger.bad(), "Requesting bad float size should result in an error."
		);
		query::Context::logger.dumpLog(false, dumped_logs);
		const auto dumped_logs_str = dumped_logs.str();
		assertTrue(
			dumped_logs_str.find("Invalid size of float type") != decltype(dumped_logs_str)::npos,
			"Logs should contain mention of invalid float size."
		);
	}

public:
	~HigherTypeSystemErrorTest() override = default;
};

TESTER_COMMON_MAIN("/compiler/typesystem/tests/")
