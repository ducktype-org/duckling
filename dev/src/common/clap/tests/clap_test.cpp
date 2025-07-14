#include <clap/clap.hpp>
#include <clap/exceptions.hpp>
#include <clap/param_builder.hpp>
#include <tester/tester.hpp>

#include <array>

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

		// Subcommands tests.
		TESTER_ADD_TEST(subcommandBasicTest);
		TESTER_ADD_TEST(subcommandNestedTest);
		TESTER_ADD_TEST(globalOptionsTest);
		TESTER_ADD_TEST(preHandlerAndHandlerExecutionOrder);
		TESTER_ADD_TEST(requiredParameterValidation);
		TESTER_ADD_TEST(noCommandSpecifiedError);
		TESTER_ADD_TEST(executeReturnValueTest);
		TESTER_ADD_TEST(conditionalParameterTest);
		TESTER_ADD_TEST(mixedGlobalAndLocalParameters);
	}

private:
	void simpleTest() {
		auto       par = clap::Clap("prog");
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
		auto par = clap::Clap("prog")
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
		auto par = clap::Clap("prog")
		               .setDefaultValueParser(clap::IntParser::make())
		               .addGlobalParameter(
						   clap::ParamBuilder::ofValue(clap::IntParser::make())
							   .addShortName('n')
							   .addLongName("nnn")
							   .addShortDesc("Desc")
							   .required()
							   .build()
					   );
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
		auto par = clap::Clap("prog")
		               .setDefaultValueParser(clap::IntParser::make())
		               .addGlobalParameter(
						   clap::ParamBuilder::ofFlag()
							   .addShortName('f')
							   .addLongName("flag")
							   .addShortDesc("Desc")
							   .build()
					   );
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
			= clap::Clap("prog")
		          .setDefaultValueParser(clap::IntParser::make())
		          .addGlobalParameter(
					  clap::ParamBuilder::ofFlag().addShortName('a').addShortDesc("Desc").build()
				  )
		          .addGlobalParameter(
					  clap::ParamBuilder::ofFlag().addShortName('b').addShortDesc("Desc").build()
				  )
		          .addGlobalParameter(
					  clap::ParamBuilder::ofValue(clap::IntParser::make())
						  .addShortName('c')
						  .addShortDesc("Desc")
						  .build()
				  );

		std::array argv{ "./prog", "-abc", "-123" };
		auto       res = par.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(2, res.getFlagCount());
		ASSERT_EQUAL(1, res.getNamedParameterCount());

		ASSERT_EQUAL(true, res.isFlag('a'));
		ASSERT_EQUAL(true, res.isFlag('b'));
		ASSERT_EQUAL(-123, res.getValue<i64>('c').value());
	}

	void weirdCases() {
		auto       par = clap::Clap("prog");
		std::array argv{ "./prog", "--" };

		assertThrows<clap::exceptions::ExpectedParameterIdentifier>(
			par.parse(argv.size(), argv.data()),
			"Should throw ExpectedParameterIdentifier exception."
		);
	}

	void noDefaultValueParser() {
		auto clap = clap::Clap("prog")
		                .addPositional(clap::IntParser::make())
		                .setDefaultValueParser(nullptr);
		ASSERT_EQUAL(true, nullptr == clap.getDefaultValueParser());  // Sketchy

		std::array argv{ "./prog", "-123" };
		auto       res = clap.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(0, res.getExtraParameterCount());
		ASSERT_EQUAL(1, res.getPositionalParameterCount());
		ASSERT_EQUAL(-123, res.getPositional<i64>(0));

		std::array argv2{ "./prog", "-123", "1231", "test" };
		assertThrows<clap::exceptions::ExpectedParameterIdentifier>(
			clap.parse(argv2.size(), argv2.begin()),
			"Should throw NoDefaultValueParser exception."
		);
	}

	void escapingTest() {
		auto clap = clap::Clap("prog").addGlobalParameter(
			clap::ParamBuilder::ofValue(clap::StringParser::make())
				.addShortName('f')
				.addShortDesc("test")
				.build()
		);

		std::array argv{ "./prog", "-f", "a \" b" };
		auto       res = clap.parse(argv.size(), argv.begin());
		ASSERT_EQUAL("a \" b", *res.getValue<std::string>('f'));
	}

	void subcommandBasicTest() {
		auto clap = clap::Clap("prog").addSubcommand(
			clap::Command("test", "A test command")
				.add(clap::ParamBuilder::ofFlag().addShortName('f').build())
		);

		std::array argv{ "./prog", "test", "-f" };
		auto       res = clap.parse(argv.size(), argv.data());

		ASSERT_EQUAL("test", res.getMatchedCommand()->getName());
		ASSERT_EQUAL("A test command", res.getMatchedCommand()->getDescription());
		ASSERT_EQUAL(true, res.isFlag('f'));

		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(2, path.size());
		ASSERT_EQUAL("prog", path[0]->getName());
		ASSERT_EQUAL("test", path[1]->getName());
	}

	void subcommandNestedTest() {
		auto clap = clap::Clap("git").addSubcommand(
			clap::Command("remote", "Manage remotes")
				.addSubcommand(
					clap::Command("add", "Add a remote")
						.addPositional(clap::StringParser::make("name"))
						.addPositional(clap::StringParser::make("url"))
				)
		);

		std::array argv{ "./git", "remote", "add", "origin", "http://git.com" };
		auto       res = clap.parse(argv.size(), argv.data());

		ASSERT_EQUAL("add", res.getMatchedCommand()->getName());
		ASSERT_EQUAL(2, res.getPositionalParameterCount());
		ASSERT_EQUAL("origin", res.getPositional<std::string>(0));
		ASSERT_EQUAL("http://git.com", res.getPositional<std::string>(1));

		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(3, path.size());
		ASSERT_EQUAL("git", path[0]->getName());
		ASSERT_EQUAL("remote", path[1]->getName());
		ASSERT_EQUAL("add", path[2]->getName());
	}

	void globalOptionsTest() {
		auto clap
			= clap::Clap("prog")
		          .addGlobalParameter(clap::ParamBuilder::ofFlag().addLongName("verbose").build())
		          .addSubcommand(
					  clap::Command("upload", "Upload a file")
						  .addPositional(clap::StringParser::make("file"))
				  );

		std::array argv{ "./prog", "--verbose", "upload", "data.zip" };
		auto       res = clap.parse(argv.size(), argv.data());

		ASSERT_EQUAL("upload", res.getMatchedCommand()->getName());
		ASSERT_EQUAL(true, res.isFlag("verbose"));
		ASSERT_EQUAL("data.zip", res.getPositional<std::string>(0));
	}

	void preHandlerAndHandlerExecutionOrder() {
		std::vector<std::string> call_order;
		auto                     clap = clap::Clap("prog")
		                .setPreHandler([&](const clap::ParsingResult&) -> int {
							call_order.emplace_back("pre_handler");
							return 420;
						})
		                .addSubcommand(
							clap::Command("cmd", "A command")
								.setHandler([&](const clap::ParsingResult&) -> int {
									call_order.emplace_back("handler");
									return 123;
								})
						);

		std::array argv{ "./prog", "cmd" };
		int        exit_code = clap.execute(argv.size(), argv.data());

		ASSERT_EQUAL(123, exit_code);
		ASSERT_EQUAL(2, call_order.size());
		ASSERT_EQUAL("pre_handler", call_order[0]);
		ASSERT_EQUAL("handler", call_order[1]);
	}

	void requiredParameterValidation() {
		auto clap = clap::Clap("prog").addSubcommand(
			clap::Command("login", "Log in")
				.add(
					clap::ParamBuilder::ofValue(clap::StringParser::make())
						.addLongName("user")
						.required()
						.build()
				)
		);

		// Missing --user param.
		std::array argv_bad{ "./prog", "login" };
		assertThrows<clap::exceptions::MissingRequiredParameter>(
			clap.parse(argv_bad.size(), argv_bad.data()),
			"Clap did not find a missing required parameter."
		);

		std::array argv_ok{ "./prog", "login", "--user", "admin" };
		auto       res = clap.parse(argv_ok.size(), argv_ok.data());

		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(2, path.size());
		ASSERT_EQUAL("prog", path[0]->getName());
		ASSERT_EQUAL("test", path[1]->getName());
		ASSERT_EQUAL("test", res.getMatchedCommand()->getName());
		ASSERT_EQUAL("admin", res.getValue<std::string>("user").value());
	}

	void noCommandSpecifiedError() {
		auto clap = clap::Clap("prog").addSubcommand(clap::Command("test", "desc"));

		std::array argv{ "./prog" };
		assertThrows<clap::exceptions::ClapException>( // TODOP: Is that a correct error?
			clap.execute(argv.size(), argv.data()), "Clap did not find a not specified subcommand."
		);
	}

	void executeReturnValueTest() {
		auto       clap = clap::Clap("prog").setPreHandler([](auto&) { return 42; });
		std::array argv{ "./prog" };
		int        exit_code = clap.execute(argv.size(), argv.data());
		ASSERT_EQUAL(42, exit_code);
	}

	void conditionalParameterTest() {
		auto clap = clap::Clap("prog")
		                .addGlobalParameter(clap::ParamBuilder::ofFlag().addLongName("a").build())
		                .addGlobalParameter(
							clap::ParamBuilder::ofFlag()
								.addLongName("b")
								.conditional(
									[](const clap::ParsingResult& res) { return res.isFlag("a"); },
									"Flag --b requires flag --a"
								)
								.build()
						);

		std::array argv1{ "./prog", "--b" };
		assertThrows<clap::exceptions::MissingConditionalParameter>( // TODOP: Is that a correct error?
			clap.parse(argv1.size(), argv1.data()), "Clap did not find a missing conditional parameter."
		);

		std::array argv2{ "./prog", "--a", "--b" };
		auto       res = clap.parse(argv2.size(), argv2.data());

		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(1, path.size());
		ASSERT_EQUAL("prog", path[0]->getName());
		ASSERT_EQUAL("prog", res.getMatchedCommand()->getName());
		ASSERT_EQUAL(true, res.isFlag("a"));
		ASSERT_EQUAL(true, res.isFlag("b"));
	}

	void mixedGlobalAndLocalParameters() {
		auto clap = clap::Clap("prog")
		                .addGlobalParameter(
							clap::ParamBuilder::ofValue(clap::StringParser::make())
								.addLongName("global")
								.build()
						)
		                .addSubcommand(
							clap::Command("cmd", "desc")
								.add(
									clap::ParamBuilder::ofValue(clap::StringParser::make())
										.addLongName("local")
										.build()
								)
						);

		std::array argv{ "./app", "--global", "g_val", "cmd", "--local", "l_val" };
		auto       res = clap.parse(argv.size(), argv.data());

		ASSERT_EQUAL("g_val", res.getValue<std::string>("global").value());
		ASSERT_EQUAL("l_val", res.getValue<std::string>("local").value());
	}
};

TESTER_COMMON_MAIN("/src/common/clap/tests/");
