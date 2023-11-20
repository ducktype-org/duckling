#include <tester/tester.hpp>
#include <clap/clap.hpp>
#include <clap/parsing_result.hpp>
#include <array>
#include "clap/exceptions.hpp"

class ClapParserTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapParserTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Parser Tester") {
		TESTER_ADD_TEST(intParserTest);
		TESTER_ADD_TEST(stringParserTest);
	}

private:
	static i64 parseInt(const std::string& str) {
		return std::any_cast<i64>(clap::IntParser::make()->parse(0, str).value);
	}

	static std::string parseString(const std::string& str) {
		return std::any_cast<std::string>(clap::StringParser::make()->parse(0, str).value);
	}

	void intParserTest() {
		ASSERT_EQUAL(123, parseInt("123"));
		ASSERT_EQUAL(-9, parseInt("-9"));
		ASSERT_EQUAL(3, parseInt("3 SHOULD NOT PARSE AFTER SPACE"));
		ASSERT_EQUAL(-200, parseInt("-200\tOR ANY OTHER WHITE SPACE"));
		ASSERT_EQUAL(0, parseInt("-0\n"));

		auto parsed = clap::IntParser::make()->parse(0, "123 123");
		ASSERT_EQUAL("123", parsed.raw_source);
		ASSERT_EQUAL(3, parsed.position);

		auto parsed2 = clap::IntParser::make()->parse(0, "123");
		ASSERT_EQUAL("123", parsed2.raw_source);
		ASSERT_EQUAL(3, parsed2.position);

		assertThrows<clap::exceptions::ValueParsingException>(
			[&]() { parseInt("str"); }, "Cannot parse str to int"
		);
	}

	void stringParserTest() {
		ASSERT_EQUAL("str", parseString("str"));

		auto parsed = clap::StringParser::make()->parse(0, "str str");
		ASSERT_EQUAL("str", parsed.raw_source);
		ASSERT_EQUAL(3, parsed.position);

		auto parsed2 = clap::StringParser::make()->parse(0, "str");
		ASSERT_EQUAL("str", parsed2.raw_source);
		ASSERT_EQUAL(3, parsed2.position);

		auto parsed3 = clap::StringParser::make()->parse(0, "\"test1 test2\" test3");
		ASSERT_EQUAL("test1 test2", std::any_cast<std::string>(parsed3.value));
		ASSERT_EQUAL(13, parsed3.position);
		ASSERT_EQUAL("test1 test2", parsed3.raw_source);

		auto parsed4 = clap::StringParser::make()->parse(0, R"("val: \"1\"" test3)");
		ASSERT_EQUAL("val: \"1\"", std::any_cast<std::string>(parsed4.value));
		ASSERT_EQUAL(12, parsed4.position);
		ASSERT_EQUAL("val: \"1\"", parsed4.raw_source);

		auto parsed5 = clap::StringParser::make()->parse(0, R"("")");
		ASSERT_EQUAL("", std::any_cast<std::string>(parsed5.value));
		ASSERT_EQUAL(2, parsed5.position);
		ASSERT_EQUAL("", parsed5.raw_source);
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
