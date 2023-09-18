#include <clap/clap.hpp>
#include <tester/tester.hpp>

using namespace clap;

class ClapTestSimple: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapTestSimple

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Simple Test") {
		TESTER_ADD_TEST(testLongValuePresent);
		TESTER_ADD_TEST(testLongValueMissing);
	}

private:
	Config getConfig() {
		return Config()
		    .add(ParameterConfig("long").description(
				"Long no short argument-less optional parameter"))
		    .add(ParameterConfig('c')
		             .description("Short no long optional parameter with argument")
		             .with_value("c_value", "c_default"))
		    .add(ParameterConfig('r')
		             .description("Long and short required parameter")
		             .long_name("req")
		             .required("r_value"));
	}

	void testLongValuePresent() {
		Config        config     = Config().add(ParameterConfig("long"));

		const char*   argv[2]    = { "name", "--long" };
		ParametersMap parameters = config.parse(CLIArgs{ 2, argv });
		assert(parameters.contains("long"), "Expected parameter '--long', but missing");
	}

	void testLongValueMissing() {
		Config        config     = Config().add(ParameterConfig("long"));

		const char*   argv[1]    = { "name" };
		ParametersMap parameters = config.parse(CLIArgs{ 1, argv });
		assert(!parameters.contains("long"), "Parameter '--long' not expected, but found");
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
