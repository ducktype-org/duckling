#include <clap/exceptions.hpp>
#include <config/config.hpp>
#include <tester/tester.hpp>

class ConfigTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConfigTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testConfig); }

private:
	auto parseOpts(const std::vector<const char*>& data) {
		auto clap = config::standardOptions();
		return config::configureWith(clap, { data.size(), data.data() });
	}

	void testConfig() {
		{
			auto opts = parseOpts({ "./prog" });
			ASSERT_EQUAL(opts.getArgs(), "");

			assertThrows<clap::exceptions::HelpException>(
				[&]() { parseOpts({ "./prog", "--help" }); }, "No help thrown"
			);

			assertThrows<clap::exceptions::HelpException>(
				[&]() { parseOpts({ "./prog", "lex", "--help" }); }, "No help thrown"
			);

			assertThrows<clap::exceptions::HelpException>(
				[&]() { parseOpts({ "./prog", "-h" }); }, "No help thrown"
			);
		}

		{
			auto opts = parseOpts({ "./prog", "--logger-cerr" });
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), true);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), false);
		}

		{
			auto opts = parseOpts({ "./prog", "--lexer-cerr" });
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), false);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), true);
		}

		{
			auto opts = parseOpts({ "./prog", "--logger-cerr", "--lexer-cerr" });
			ASSERT_EQUAL(opts.isFlag("logger-cerr"), true);
			ASSERT_EQUAL(opts.isFlag("lexer-cerr"), true);
		}
	}
};

TESTER_COMMON_MAIN("src/compiler/config/tests/");
