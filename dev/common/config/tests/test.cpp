#include <config/config.hpp>
#include <printer/printer.hpp>
#include <tester/tester.hpp>

class ConfigTestSimple: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConfigTestSimple

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Config Simple Test") {
		TESTER_ADD_TEST(simpleTest1);
		TESTER_ADD_TEST(simpleTest2);
		TESTER_ADD_TEST(simpleTest3);
		TESTER_ADD_TEST(simpleErrorTest);
		TESTER_ADD_TEST(simpleGetHelpMessageTest);
	}

private:
	static config::ConfigOptions makeParamsConfig() {
		config::ConfigOptions out;
		out.addOption("long1", "long option without parameter");
		out.addOption("long2", "s2", "long and short option without parameter");
		out.addOption("intOption",
		              "i",
		              config::ParamType::Always,
		              config::makeParser<config::IntParser>(),
		              "long and short option with i32 parameter");
		out.addOption("strOption",
		              "s",
		              config::ParamType::Always,
		              config::makeParser<config::StringParser>(),
		              "long and short option with str parameter");
		out.addOption("intOption2",
		              config::ParamType::Always,
		              config::makeParser<config::IntParser>(),
		              "long option with i32 parameter");
		return out;
	}

	void simpleTest1() {
		const std::vector<base::RawView> options     = {};
		auto                             parsed_args = config::parse(makeParamsConfig(), options);

		assert(parsed_args.wasOption("long1") == false,
		       "Option was parsed, but was not passed (1)");
		assert(parsed_args.wasOption("long2") == false,
		       "Option was parsed, but was not passed (2)");
		assert(parsed_args.wasOption("intOption") == false,
		       "Option was parsed, but was not passed (3)");
		assert(parsed_args.wasOption("strOption") == false,
		       "Option was parsed, but was not passed (4)");
		assert(parsed_args.wasOption("intOption2") == false,
		       "Option was parsed, but was not passed (5)");
	}

	void simpleTest2() {
		const std::vector<base::RawView> options = {
			"-s2", "--intOption", "123", "-s", "aa",
		};
		auto parsed_args = config::parse(makeParamsConfig(), options);

		assert(parsed_args.wasOption("long1") == false,
		       "Option was parsed, but was not passed (1)");
		assert(parsed_args.wasOption("intOption2") == false,
		       "Option was parsed, but was not passed (5)");

		assert(parsed_args.wasOption("long2") == true, "Option was not parsed, but was passed (2)");
		assert(parsed_args.wasOption("intOption") == true,
		       "Option was not parsed, but was passed (3)");
		assert(parsed_args.wasOption("strOption") == true,
		       "Option was not parsed, but was passed (4)");

		assert(parsed_args.getRawValue("strOption") == "aa", "Incorrect strOption raw argument");
		assert(parsed_args.getValue<std::string>("strOption") == "aa",
		       "Incorrect strOption argument");

		assert(parsed_args.getRawValue("intOption") == "123", "Incorrect intOption raw argument");
		assert(parsed_args.getValue<i64>("intOption") == 123, "Incorrect intOption argument");
	}

	void simpleTest3() {
		const std::vector<base::RawView> options
			= { "--strOption", "hello there", "--intOption2", "0", "--long1" };
		auto parsed_args = config::parse(makeParamsConfig(), options);

		assert(parsed_args.wasOption("long2") == false,
		       "Option was parsed, but was not passed (2)");
		assert(parsed_args.wasOption("intOption") == false,
		       "Option was parsed, but was not passed (3)");

		assert(parsed_args.wasOption("long1") == true, "Option was not parsed, but was passed (1)");
		assert(parsed_args.wasOption("strOption") == true,
		       "Option was not parsed, but was passed (4)");
		assert(parsed_args.wasOption("intOption2") == true,
		       "Option was not parsed, but was passed (5)");

		assert(parsed_args.getRawValue("strOption") == "hello there",
		       "Incorrect strOption raw argument");
		assert(parsed_args.getValue<std::string>("strOption") == "hello there",
		       "Incorrect strOption argument");

		assert(parsed_args.getRawValue("intOption2") == "0", "Incorrect intOption2 raw argument");
		assert(parsed_args.getValue<i64>("intOption2") == 0, "Incorrect intOption2 argument");

		assertThrows<config::BadOptionAccess>([&]() { parsed_args.wasOption("abc"); },
		                                      "No error generated on bad option access (1)");

		assertThrows<config::BadOptionAccess>([&]() { parsed_args.wasOption("s"); },
		                                      "No error generated on bad option access (2)");

		assertThrows<config::BadOptionAccess>([&]() { parsed_args.getRawValue("long1"); },
		                                      "No error generated on bad option access (3)");

		assertThrows<config::BadOptionAccess>([&]() { parsed_args.getValue<std::string>("long1"); },
		                                      "No error generated on bad option access (4)");

		assertThrows<config::BadOptionAccess>([&]() { parsed_args.getValue<std::string>("abc"); },
		                                      "No error generated on bad option access (5)");

		assertThrows<config::BadOptionAccess>([&]() { parsed_args.getValue<i64>("intOption"); },
		                                      "No error generated on bad option access (6)");
	}

	void simpleErrorTest() {
		{
			const std::vector<base::RawView> options = { "--intOption2", "123aaaaa" };

			assertThrows<config::WrongParamValue>(
				[&]() { config::parse(makeParamsConfig(), options); },
				"No error generated on bad parameter value (1)");
		}
		{
			const std::vector<base::RawView> options = { "--intOption2", "a1" };

			assertThrows<config::WrongParamValue>(
				[&]() { config::parse(makeParamsConfig(), options); },
				"No error generated on bad parameter value (2)");
		}
		{
			const std::vector<base::RawView> options = { "--intOption2", "--" };
			assertThrows<config::WrongParamValue>(
				[&]() { config::parse(makeParamsConfig(), options); },
				"No error generated on bad parameter value (3)");
		}
	}

	void simpleGetHelpMessageTest() {
		auto             param_config = makeParamsConfig();
		printer::Message message({ "" });
		param_config.generateOptionDesc(message);

		std::stringstream out;
		message.print(out);

		// @TODO: sam tests here
	}
};

TESTER_COMMON_MAIN("/common/config/tests/");
