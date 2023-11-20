#include <tester/tester.hpp>
#include <clap/clap.hpp>
#include <clap/param_builder.hpp>
#include <array>

class ClapTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapTester


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Tester") {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(positionalTest);
		TESTER_ADD_TEST(namedTest);
		TESTER_ADD_TEST(flagTest);
	}

private:
	void simpleTest() {
		auto       par = clap::Clap();
		std::array argv{ "./prog", "1", "test", "3" };
		auto       res = par.parse(argv.size(), argv.begin());

		ASSERT_EQUAL("prog", res.getFilePath());
		ASSERT_EQUAL("1 test 3", res.getArgs());
		ASSERT_EQUAL(0, res.getNamedParameterCount());
		ASSERT_EQUAL(0, res.getFlagCount());
		ASSERT_EQUAL(0, res.getPositionalParameterCount());
		ASSERT_EQUAL(3, res.getExtraParameterCount());

		ASSERT_EQUAL("1", *res.getExtra<std::string>(0));
		ASSERT_EQUAL("test", *res.getExtra<std::string>(1));
		ASSERT_EQUAL("3", *res.getExtra<std::string>(2));
	}

	void positionalTest() {
		auto par = clap::Clap()
		               .addPositional(clap::StringParser::make())
		               .addPositional(clap::IntParser::make());

		std::array argv{ "prog", "test", "2" };
		auto       res = par.parse(argv.size(), argv.begin());

		ASSERT_EQUAL("prog", res.getFilePath());  // now, prog is invoked without "./"
		ASSERT_EQUAL(0, res.getNamedParameterCount());
		ASSERT_EQUAL(0, res.getFlagCount());
		ASSERT_EQUAL(2, res.getPositionalParameterCount());
		ASSERT_EQUAL(0, res.getExtraParameterCount());

		ASSERT_EQUAL("test", *res.getPositional<std::string>(0));
		ASSERT_EQUAL(2, *res.getPositional<i64>(1));
	}

	void namedTest() {
		auto par = clap::Clap()
		               .setDefaultParser(clap::IntParser::make())
		               .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
		                        .addShortName('n')
		                        .addLongName("nnn")
		                        .addShortDesc("Desc")
		                        .required()
		                        .build());
		std::array argv{ "./prog", "-1", "-n", "-20" };
		auto       res = par.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(1, res.getNamedParameterCount());
		ASSERT_EQUAL(0, res.getFlagCount());
		ASSERT_EQUAL(0, res.getPositionalParameterCount());
		ASSERT_EQUAL(1, res.getExtraParameterCount());

		ASSERT_EQUAL(-1, *res.getExtra<i64>(0));
		ASSERT_EQUAL(-20, *res.getValue<i64>('n'));
		ASSERT_EQUAL(-20, *res.getValue<i64>("nnn"));
		ASSERT_EQUAL("-20", *res.getRaw('n'));
	}

	void flagTest() {
		auto par = clap::Clap()
		               .setDefaultParser(clap::IntParser::make())
		               .add(clap::ParamBuilder::ofFlag()
		                        .addShortName('f')
		                        .addLongName("flag")
		                        .addShortDesc("Desc")
		                        .build());
		std::array argv{ "./prog" };
		auto       res = par.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(0, res.getNamedParameterCount());
		ASSERT_EQUAL(0, res.getFlagCount());
		ASSERT_EQUAL(0, res.getPositionalParameterCount());
		ASSERT_EQUAL(0, res.getExtraParameterCount());

		std::array argv2{ "./prog", "-f", "123" };
		auto       res2 = par.parse(argv2.size(), argv2.begin());

		ASSERT_EQUAL(0, res2.getNamedParameterCount());
		ASSERT_EQUAL(1, res2.getFlagCount());
		ASSERT_EQUAL(0, res2.getPositionalParameterCount());
		ASSERT_EQUAL(1, res2.getExtraParameterCount());

		ASSERT_EQUAL(123, *res2.getExtra<i64>(0));
		ASSERT_EQUAL(true, res2.isFlag('f'));
		ASSERT_EQUAL(true, res2.isFlag("flag"));
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
