#include <array>
#include <clap/clap.hpp>
#include <clap/exceptions.hpp>
#include <clap/param_builder.hpp>
#include <tester/tester.hpp>

class ClapTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapTester


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(positionalTest);
		TESTER_ADD_TEST(namedTest);
		TESTER_ADD_TEST(flagTest);
		TESTER_ADD_TEST(multipleFlagsTest);
		TESTER_ADD_TEST(weirdCases);
		TESTER_ADD_TEST(noDefaultValueParser);
		TESTER_ADD_TEST(escapingTest);
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

		ASSERT_EQUAL("test", res.getPositional<std::string>(0));
		ASSERT_EQUAL(2, res.getPositional<i64>(1));
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

		ASSERT_EQUAL(-1, res.getExtra<i64>(0).value());
		ASSERT_EQUAL(-20, res.getValue<i64>('n').value());
		ASSERT_EQUAL(-20, res.getValue<i64>("nnn").value());
		ASSERT_EQUAL("-20", res.getRaw('n').value());

		ASSERT_EQUAL(true, res.isParam('n'));
		ASSERT_EQUAL(false, res.isParam("sth"));
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

		ASSERT_EQUAL(123, res2.getExtra<i64>(0).value());
		ASSERT_EQUAL(true, res2.isFlag('f'));
		ASSERT_EQUAL(true, res2.isFlag("flag"));
	}

	void multipleFlagsTest() {
		auto par
			= clap::Clap()
		          .setDefaultParser(clap::IntParser::make())
		          .add(clap::ParamBuilder::ofFlag().addShortName('a').addShortDesc("Desc").build())
		          .add(clap::ParamBuilder::ofFlag().addShortName('b').addShortDesc("Desc").build())
		          .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
		                   .addShortName('c')
		                   .addShortDesc("Desc")
		                   .build());

		std::array argv{ "./prog", "-abc", "-123" };
		auto       res = par.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(2, res.getFlagCount());
		ASSERT_EQUAL(1, res.getNamedParameterCount());

		ASSERT_EQUAL(true, res.isFlag('a'));
		ASSERT_EQUAL(true, res.isFlag('b'));
		ASSERT_EQUAL(-123, res.getValue<i64>('c').value());
	}

	void weirdCases() {
		auto       par = clap::Clap();
		std::array argv{ "./prog", "--" };
		bool       exception = false;
		try {
			auto res = par.parse(argv.size(), argv.begin());
		} catch (clap::exceptions::ExpectedParameterIdentifier& _) { exception = true; }
		assertEqual(true, exception, "Should throw ExpectedParameterIdentifier exception.");
	}

	void noDefaultValueParser() {
		auto clap = clap::Clap().addPositional(clap::IntParser::make()).setDefaultParser(nullptr);
		ASSERT_EQUAL(true, nullptr == clap.getDefaultValueParser());  // Sketchy

		std::array argv{ "./prog", "-123" };
		auto       res = clap.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(0, res.getExtraParameterCount());
		ASSERT_EQUAL(1, res.getPositionalParameterCount());
		ASSERT_EQUAL(-123, res.getPositional<i64>(0));

		std::array argv2{ "./prog", "-123", "1231", "test" };
		bool       caught = false;
		try {
			auto res2 = clap.parse(argv2.size(), argv2.begin());
		} catch (clap::exceptions::NoDefaultValueParser& e) { caught = true; }
		ASSERT_EQUAL(true, caught);
	}

	void escapingTest() {
		auto clap = clap::Clap().add(clap::ParamBuilder::ofValue(clap::StringParser::make())
		                                 .addShortName('f')
		                                 .addShortDesc("test")
		                                 .build());

		std::array argv{ "./prog", "-f", "a \" b" };
		auto       res = clap.parse(argv.size(), argv.begin());
		ASSERT_EQUAL("a \" b", *res.getValue<std::string>('f'));
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
