#include <tester/tester.hpp>
#include <clap/clap.hpp>
#include <clap/parsing_result.hpp>
#include <array>

class ClapTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Tester") { TESTER_ADD_TEST(simpleTest); }

private:
	void simpleTest() {
		auto                       par  = clap::Clap();
		std::array<const char*, 4> argv = { "./prog", "1", "test", "3" };
		auto                       res  = par.parse(argv.size(), argv.begin());

		assert(
			res.getPositional<std::string>(0).has_value(),
			"ParsingResult does not contain the first positional parameter!"
		);
		ASSERT_EQUAL("1", res.getPositional<std::string>(0).value());

		assert(
			res.getPositional<std::string>(1).has_value(),
			"ParsingResult does not contain the second positional parameter!"
		);
		ASSERT_EQUAL("test", res.getPositional<std::string>(1).value());

		assert(
			res.getPositional<std::string>(2).has_value(),
			"ParsingResult does not contain the third positional parameter!"
		);
		ASSERT_EQUAL("3", res.getPositional<std::string>(2).value());
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
