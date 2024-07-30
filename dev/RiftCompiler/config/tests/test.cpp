#include <tester/tester.hpp>
#include <config/config.hpp>
#include <clap/exceptions.hpp>

class ConfigTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConfigTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Config Test") {
		TESTER_ADD_TEST(testConfig);
	}

private:

	template<int size>
	auto parseOpts(const std::array<const char*, size>& data) {
		auto clap = config::standardOptions();
		return config::configureWith(clap, {size, data.data()});
	}

	void testConfig() {
		{
			auto opts = parseOpts<0>({});
			ASSERT_EQUAL(opts.getArgs(), "");

			assertThrows<clap::exceptions::HelpException>([&]() {
				parseOpts<1>({"--help"});
			}, "No help thrown");

			assertThrows<clap::exceptions::HelpException>([&]() {
				parseOpts<1>({"lex --help"});
			}, "No help thrown");

			assertThrows<clap::exceptions::HelpException>([&]() {
				parseOpts<1>({"-h"});
			}, "No help thrown");
		}

		{
			auto opts = parseOpts<1>({"--logger-cerr"});
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), true);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), false);
		}

		{
			auto opts = parseOpts<1>({"--lexer-cerr"});
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), false);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), true);
		}

		{
			auto opts = parseOpts<2>({"--logger-cerr", "--lexer-cerr"});
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), true);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), true);
		}

		{
			auto opts = parseOpts<3>({"--logger-cerr", "--lexer-cerr", "--help"});
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), true);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), true);
			ASSERT_EQUAL(opts.isFlag("help"), false);
		}
	}
	
};

TESTER_COMMON_MAIN("/RiftCompiler/config/tests/");
