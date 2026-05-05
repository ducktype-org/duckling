#include <clah/clah.hpp>
#include <tester/tester.hpp>

#include <array>

class ClahTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClahTester


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
		TESTER_ADD_TEST(executeReturnValueTest);
		TESTER_ADD_TEST(coexistingSubcommandsAndPositionals);
		TESTER_ADD_TEST(duplicateSubcommand);
		TESTER_ADD_TEST(conditionalParameterTest);
		TESTER_ADD_TEST(mixedGlobalAndLocalParameters);
		TESTER_ADD_TEST(argumentParsingTest);
		TESTER_ADD_TEST(categoryParserTest);
		TESTER_ADD_TEST(categoryListParserTest);
	}

private:
	void simpleTest() {
		auto       par = clah::Clah("prog");
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
		auto par = clah::Clah("prog")
		               .addPositional(clah::StringParser::make())
		               .addPositional(clah::IntParser::make());

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
		auto par = clah::Clah("prog")
		               .setDefaultValueParser(clah::IntParser::make())
		               .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
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
		auto par = clah::Clah("prog")
		               .setDefaultValueParser(clah::IntParser::make())
		               .add(clah::ParamBuilder::ofFlag()
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
			= clah::Clah("prog")
		          .setDefaultValueParser(clah::IntParser::make())
		          .add(clah::ParamBuilder::ofFlag().addShortName('a').addShortDesc("Desc").build())
		          .add(clah::ParamBuilder::ofFlag().addShortName('b').addShortDesc("Desc").build())
		          .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
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
		auto       par = clah::Clah("prog");
		std::array argv{ "./prog", "--" };

		assertThrows<clah::exceptions::ExpectedParameterIdentifier>(
			[&]() { par.parse(argv.size(), argv.data()); },
			"Should throw ExpectedParameterIdentifier exception."
		);
	}

	void noDefaultValueParser() {
		auto clah = clah::Clah("prog")
		                .addPositional(clah::IntParser::make())
		                .setDefaultValueParser(nullptr);
		ASSERT_EQUAL(true, nullptr == clah.getDefaultValueParser());  // Sketchy

		std::array argv{ "./prog", "-123" };
		auto       res = clah.parse(argv.size(), argv.begin());

		ASSERT_EQUAL(0, res.getExtraParameterCount());
		ASSERT_EQUAL(1, res.getPositionalParameterCount());
		ASSERT_EQUAL(-123, res.getPositional<i64>(0));

		std::array argv2{ "./prog", "-123", "1231", "test" };
		assertThrows<clah::exceptions::NoDefaultValueParser>(
			[&]() { clah.parse(argv2.size(), argv2.begin()); },
			"Should throw NoDefaultValueParser exception."
		);
	}

	void escapingTest() {
/// We need this define because of the usages below
/// - the std::string_view can't be used with string literal to create a vector
/// - const char* can't be automatically concatenated by the compiler with the string literal
#define STR_ARG "a \" b"

		auto clah = clah::Clah("prog").add(clah::ParamBuilder::ofValue(clah::StringParser::make())
		                                       .addShortName('f')
		                                       .addLongName("file")
		                                       .addShortDesc("test")
		                                       .build());
		// The const char* has to stay because the clah.parse requires this signature.
		auto correctly_parses_argument
			= [&](const std::vector<const char*>& argv, const std::string& expected) {
				  auto res = clah.parse(argv.size(), argv.data());
				  ASSERT_EQUAL(expected, *res.getValue<std::string>('f'));
			  };
		correctly_parses_argument({ "./prog", "-f", STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "-f=", STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "-f", "=" STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "-f=" STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "--file", STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "--file=", STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "--file", "=" STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "--file=" STR_ARG }, STR_ARG);
		correctly_parses_argument({ "./prog", "--file=abc" }, "abc");
	}

	void subcommandBasicTest() {
		auto clah = clah::Clah("prog").addSubcommand(
			clah::Clah("test", "A test command")
				.add(clah::ParamBuilder::ofFlag().addShortName('f').addShortDesc("desc").build())
		);

		std::array argv{ "./prog", "test", "-f" };
		auto       res = clah.parse(argv.size(), argv.data());

		ASSERT_EQUAL("test", res.getMatchedCommand().value()->getName());
		ASSERT_EQUAL("A test command", res.getMatchedCommand().value()->getDescription());
		ASSERT_EQUAL(true, res.isFlag('f'));

		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(2, path.size());
		ASSERT_EQUAL("prog", path[0]->getName());
		ASSERT_EQUAL("test", path[1]->getName());
	}

	void subcommandNestedTest() {
		auto clah = clah::Clah("git").addSubcommand(
			clah::Clah("remote", "Manage remotes")
				.addSubcommand(clah::Clah("add", "Add a remote")
		                           .addPositional(clah::StringParser::make("name"))
		                           .addPositional(clah::StringParser::make("url")))
		);

		std::array argv{ "./git", "remote", "add", "origin", "http://git.com" };
		auto       res = clah.parse(argv.size(), argv.data());

		ASSERT_EQUAL("add", res.getMatchedCommand().value()->getName());
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
		auto clah
			= clah::Clah("prog")
		          .add(clah::ParamBuilder::ofFlag().addLongName("verbose").addShortDesc("desc").build(
				  ))
		          .addSubcommand(clah::Clah("upload", "Upload a file")
		                             .addPositional(clah::StringParser::make("file")));

		std::array argv{ "./prog", "--verbose", "upload", "data.zip" };
		auto       res = clah.parse(argv.size(), argv.data());

		ASSERT_EQUAL("upload", res.getMatchedCommand().value()->getName());
		ASSERT_EQUAL(true, res.isFlag("verbose"));
		ASSERT_EQUAL("data.zip", res.getPositional<std::string>(0));
	}

	void preHandlerAndHandlerExecutionOrder() {
		std::vector<std::string> call_order;
		auto                     clah = clah::Clah("prog")
		                .setPreHandler([&](const clah::ParsingResult&) -> int {
							call_order.emplace_back("pre_handler");
							return 421;
						})
		                .addSubcommand(clah::Clah("cmd", "A command")
		                                   .setHandler([&](const clah::ParsingResult&) -> int {
											   call_order.emplace_back("handler");
											   return 123;
										   }));

		std::array argv{ "./prog", "cmd" };
		int        exit_code = clah.execute(argv.size(), argv.data());

		ASSERT_EQUAL(123, exit_code);
		ASSERT_EQUAL(2, call_order.size());
		ASSERT_EQUAL("pre_handler", call_order[0]);
		ASSERT_EQUAL("handler", call_order[1]);
	}

	void requiredParameterValidation() {
		auto clah = clah::Clah("prog").addSubcommand(
			clah::Clah("login", "Log in")
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make())
		                 .addLongName("user")
		                 .addShortDesc("desc")
		                 .required()
		                 .build())
		);

		// Missing --user param.
		std::array argv_bad{ "./prog", "login" };
		assertThrows<clah::exceptions::MissingRequiredParameter>(
			[&]() { clah.parse(argv_bad.size(), argv_bad.data()); },
			"Clah did not find a missing required parameter."
		);

		std::array argv_ok{ "./prog", "login", "--user", "admin" };
		auto       res = clah.parse(argv_ok.size(), argv_ok.data());

		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(2, path.size());
		ASSERT_EQUAL("prog", path[0]->getName());
		ASSERT_EQUAL("login", path[1]->getName());
		ASSERT_EQUAL("login", res.getMatchedCommand().value()->getName());
		ASSERT_EQUAL("admin", res.getValue<std::string>("user").value());
	}

	void coexistingSubcommandsAndPositionals() {
		assertThrows<clah::exceptions::CoexistingPositionalAndSubcommand>(
			[&]() {
				auto clah = clah::Clah("prog").addSubcommand(
					clah::Clah("test", "desc")
						.addPositional(clah::IntParser::make())
						.addSubcommand(clah::Clah("tests", "desc"))
				);
			},
			"Clah did not find a coexisting subcommand and positional argument."
		);

		assertThrows<clah::exceptions::CoexistingPositionalAndSubcommand>(
			[&]() {
				auto clah = clah::Clah("prog").addSubcommand(
					clah::Clah("test", "desc")
						.addSubcommand(clah::Clah("tests", "desc"))
						.addPositional(clah::IntParser::make())
				);
			},
			"Clah did not find a coexisting subcommand and positional argument."
		);
	}

	void duplicateSubcommand() {
		assertThrows<clah::exceptions::DuplicateSubcommand>(
			[&]() {
				auto clah
					= clah::Clah("prog")
			              .addSubcommand(
							  clah::Clah("test", "desc").addPositional(clah::IntParser::make())
						  )
			              .addSubcommand(
							  clah::Clah("test", "desc").addPositional(clah::IntParser::make())
						  );
			},
			"Clah did not find a coexisting subcommand and positional argument."
		);
	}

	void executeReturnValueTest() {
		auto clah = clah::Clah("prog").addSubcommand(
			clah::Clah("test", "desc").setHandler([](const clah::ParsingResult&) { return 42; })
		);
		std::array argv{ "./prog", "test" };
		int        exit_code = clah.execute(argv.size(), argv.data());
		ASSERT_EQUAL(42, exit_code);
	}

	void conditionalParameterTest() {
		auto clah
			= clah::Clah("prog")
		          .add(clah::ParamBuilder::ofFlag().addShortName('a').addShortDesc("desc").build())
		          .add(clah::ParamBuilder::ofFlag()
		                   .addShortName('b')
		                   .addShortDesc("desc")
		                   .conditional(
							   [](const clah::ParsingResult& res) { return res.isFlag('a'); },
							   "Flag -b requires flag -a"
						   )
		                   .build());
		std::array argv1{ "./prog", "-b" };
		assertThrows<clah::exceptions::MissingConditionalParameter>(
			[&]() { clah.parse(argv1.size(), argv1.data()); },
			"Clah did not find a missing conditional parameter."
		);

		std::array argv2{ "./prog", "-a", "-b" };

		auto        res  = clah.parse(argv2.size(), argv2.data());
		const auto& path = res.getCommandPath();
		ASSERT_EQUAL(1, path.size());
		ASSERT_EQUAL("prog", path[0]->getName());
		ASSERT_EQUAL("prog", res.getMatchedCommand().value()->getName());
		ASSERT_EQUAL(true, res.isFlag('a'));
		ASSERT_EQUAL(true, res.isFlag('b'));
	}

	void mixedGlobalAndLocalParameters() {
		auto clah
			= clah::Clah("prog")
		          .add(clah::ParamBuilder::ofValue(clah::StringParser::make())
		                   .addLongName("global")
		                   .addShortDesc("desc")
		                   .build())
		          .addSubcommand(clah::Clah("cmd", "desc")
		                             .add(clah::ParamBuilder::ofValue(clah::StringParser::make())
		                                      .addLongName("local")
		                                      .addShortDesc("desc")
		                                      .build()));

		std::array argv{ "./prog", "--global", "g_val", "cmd", "--local", "l_val" };
		auto       res = clah.parse(argv.size(), argv.data());

		ASSERT_EQUAL("g_val", res.getValue<std::string>("global").value());
		ASSERT_EQUAL("l_val", res.getValue<std::string>("local").value());

		// Wrong order `--global` is not visible for the cmd subcommand.
		std::array argv2{ "./prog", "cmd", "--global", "g_val", "--local", "l_val" };
		assertThrows<clah::exceptions::InvalidParameterName>(
			[&]() { clah.parse(argv2.size(), argv2.data()); },
			"Clah did not find an invalid parameter name."
		);
	}

	void argumentParsingTest() {
		auto clah1 = clah::Clah()
		                 .add(clah::ParamBuilder::ofFlag()
		                          .addShortName('v')
		                          .addLongName("verbose")
		                          .addShortDesc("desc")
		                          .build())
		                 .add(clah::ParamBuilder::ofValue(clah::StringParser::make())
		                          .addLongName("config")
		                          .addShortDesc("desc")
		                          .required()
		                          .build());

		std::string args1 = "-v --config ~/.config/nvim/init.lua";
		auto        res1  = clah1.parseArgs(args1);

		ASSERT_EQUAL("", res1.getFilePath());
		ASSERT_EQUAL(args1, res1.getArgs());
		ASSERT_EQUAL(true, res1.isFlag('v'));
		ASSERT_EQUAL(true, res1.isFlag("verbose"));
		ASSERT_EQUAL("~/.config/nvim/init.lua", *res1.getValue<std::string>("config"));
		ASSERT_EQUAL(0, res1.getExtraParameterCount());

		auto clah2
			= clah::Clah()
		          .add(clah::ParamBuilder::ofFlag().addLongName("version").addShortDesc("").build())
		          .addSubcommand(
					  clah::Clah("remote", "Manage remotes")
						  .add(clah::ParamBuilder::ofFlag().addShortName('v').addShortDesc("").build(
						  ))
						  .addPositional(clah::StringParser::make("name"))
				  );

		std::string args2 = "remote -v my-origin";
		auto        res2  = clah2.parseArgs(args2);
		ASSERT_EQUAL("remote", res2.getMatchedCommand().value()->getName());
		ASSERT_EQUAL(true, res2.isFlag('v'));
		ASSERT_EQUAL(false, res2.isFlag("version"));
		ASSERT_EQUAL("my-origin", res2.getPositional<std::string>(0));
	}

	void categoryParserTest() {
		auto clah = clah::Clah("prog").add(
			clah::ParamBuilder::ofValue(clah::CategoryParser::make(std::vector<std::string>{
											"low", "medium", "high" }))
				.addLongName("priority")
				.addShortDesc("Priority level")
				.required()
				.build()
		);

		std::array argv_ok{ "./prog", "--priority", "medium" };
		auto       res_ok = clah.parse(argv_ok.size(), argv_ok.data());
		ASSERT_EQUAL("medium", *res_ok.getValue<std::string>("priority"));

		std::array argv_bad{ "./prog", "--priority", "urgent" };
		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { clah.parse(argv_bad.size(), argv_bad.data()); },
			"Clah did not reject an invalid category value."
		);
	}

	void categoryListParserTest() {
		auto clah = clah::Clah("prog").addPositional(
			clah::CategoryListParser::make(std::vector<std::string>{ "cpu", "memory", "disk" })
		);

		std::array argv_ok{ "./prog", "cpu, memory,disk" };
		auto       res_ok = clah.parse(argv_ok.size(), argv_ok.data());

		auto values = res_ok.getPositional<std::vector<std::string>>(0);
		ASSERT_EQUAL(3, values.size());
		ASSERT_EQUAL("cpu", values[0]);
		ASSERT_EQUAL("memory", values[1]);
		ASSERT_EQUAL("disk", values[2]);

		std::array argv_bad{ "./prog", "cpu,gpu" };
		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { clah.parse(argv_bad.size(), argv_bad.data()); },
			"Clah did not reject an invalid category in category-list parser."
		);
	}
};

TESTER_COMMON_MAIN("/src/common/clah/tests/");
