#include <tester/tester.hpp>
#include <clap/clap.hpp>
#include <array>

class ClapTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Tester") { TESTER_ADD_TEST(simpleTest); }

private:
	void simpleTest() {
		auto                 par  = clap::Clap().addPositional(clap::StringParser::make());
		std::array<char*, 2> argv = { "./prog", "1 test 3" };
		auto                 res  = par.parse(argv.size(), argv.begin());

		//		assert(res.getPositional<std::string>(0).value() == "1", "lol");
		auto pos1 = res.getPositional<std::string>(0);
		std::cout << *pos1 << std::endl;
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
