#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>

using namespace ts;

class TypeSystemErrorTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeSystemErrorTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("TypeSystem simple interface test") {
		TESTER_ADD_TEST(integral_size_error_test);
		TESTER_ADD_TEST(float_size_error_test);
	}

private:
	void integral_size_error_test() {
		query::detail::ContextType::logger.clear();
		assert(query::detail::ContextType::logger.good(), "Test should begin without errors.");
		query::queryEntryPoint<QueryIntegralType>({ 42 });

		std::stringstream dumped_logs;
		assert(
			query::detail::ContextType::logger.bad(),
			"Requesting bad integral size should result in an error."
		);
		query::detail::ContextType::logger.dumpLog(false, dumped_logs);
		const auto dumped_logs_str = dumped_logs.str();
		assert(
			dumped_logs_str.find("Invalid size of integral type") != dumped_logs_str.npos,
			"Logs should contain mention of invalid integral size."
		);
	}

	void float_size_error_test() {
		query::detail::ContextType::logger.clear();
		assert(query::detail::ContextType::logger.good(), "Test should begin without errors.");
		query::queryEntryPoint<QueryFloatType>({ 42 });

		std::stringstream dumped_logs;
		assert(
			query::detail::ContextType::logger.bad(),
			"Requesting bad float size should result in an error."
		);
		query::detail::ContextType::logger.dumpLog(false, dumped_logs);
		const auto dumped_logs_str = dumped_logs.str();
		assert(
			dumped_logs_str.find("Invalid size of float type") != dumped_logs_str.npos,
			"Logs should contain mention of invalid float size."
		);
	}

public:
	~TypeSystemErrorTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/typesystem/tests/")
