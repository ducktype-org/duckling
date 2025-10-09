#include <clah/exceptions.hpp>
#include <clah/parsing_result.hpp>
#include <filesystem/file.hpp>
#include <tester/tester.hpp>

class ClahParserTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClahParserTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(intParserTest);
		TESTER_ADD_TEST(stringParserTest);
		TESTER_ADD_TEST(rangeParserTest);
		TESTER_ADD_TEST(fileParserTest);
		TESTER_ADD_TEST(filePathParserTest);
	}

private:
	static i64 parseInt(const std::string& str) {
		return std::any_cast<i64>(clah::IntParser::make()->parse(0, str).value);
	}

	static std::string parseString(const std::string& str) {
		return std::any_cast<std::string>(clah::StringParser::make()->parse(0, str).value);
	}

	using Range = std::pair<i64, i64>;

	static Range parseRange(const std::string& str) {
		auto val
			= std::any_cast<clah::RangeParser::Range>(clah::RangeParser::make()->parse(0, str).value
		    );
		return { val.begin, val.end };
	}

	static fs::File parseFile(const std::string& str, const std::regex& regex = std::regex(".*")) {
		return std::any_cast<fs::File>(clah::FileParser::make(regex)->parse(0, str).value);
	}

	static fs::FilePath parseFilePath(
		const std::string& str, const std::regex& regex = std::regex(".*")
	) {
		return std::any_cast<fs::FilePath>(clah::FilePathParser::make(regex)->parse(0, str).value);
	}

	void intParserTest() {
		ASSERT_EQUAL(123, parseInt("123"));
		ASSERT_EQUAL(-9, parseInt("-9"));
		ASSERT_EQUAL(3, parseInt("3 SHOULD NOT PARSE AFTER SPACE"));
		ASSERT_EQUAL(-200, parseInt("-200\tOR ANY OTHER WHITE SPACE"));
		ASSERT_EQUAL(0, parseInt("-0\n"));

		auto parsed = clah::IntParser::make()->parse(0, "123 123");
		ASSERT_EQUAL("123", parsed.raw_source);
		ASSERT_EQUAL(3, parsed.position);

		auto parsed2 = clah::IntParser::make()->parse(0, "123");
		ASSERT_EQUAL("123", parsed2.raw_source);
		ASSERT_EQUAL(3, parsed2.position);

		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseInt("str"); }, "Cannot parse str to int"
		);
	}

	void stringParserTest() {
		ASSERT_EQUAL("str", parseString("str"));

		auto parsed = clah::StringParser::make()->parse(0, "str str");
		ASSERT_EQUAL("str", parsed.raw_source);
		ASSERT_EQUAL(3, parsed.position);

		auto parsed2 = clah::StringParser::make()->parse(0, "str");
		ASSERT_EQUAL("str", parsed2.raw_source);
		ASSERT_EQUAL(3, parsed2.position);

		auto parsed3 = clah::StringParser::make()->parse(0, "\"test1 test2\" test3");
		ASSERT_EQUAL("test1 test2", std::any_cast<std::string>(parsed3.value));
		ASSERT_EQUAL(13, parsed3.position);
		ASSERT_EQUAL("test1 test2", parsed3.raw_source);

		auto parsed4 = clah::StringParser::make()->parse(0, R"("val: \"1\"" test3)");
		ASSERT_EQUAL("val: \"1\"", std::any_cast<std::string>(parsed4.value));
		ASSERT_EQUAL(12, parsed4.position);
		ASSERT_EQUAL("val: \"1\"", parsed4.raw_source);

		auto parsed5 = clah::StringParser::make()->parse(0, R"("")");
		ASSERT_EQUAL("", std::any_cast<std::string>(parsed5.value));
		ASSERT_EQUAL(2, parsed5.position);
		ASSERT_EQUAL("", parsed5.raw_source);
	}

	void rangeParserTest() {
		ASSERT_EQUAL(Range(1, 2), parseRange("1..2"));
		ASSERT_EQUAL(Range(-1, 2), parseRange("-1..2"));
		ASSERT_EQUAL(Range(5, 2), parseRange("5..2"));
		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseRange("1 .. 3"); }, "Should throw on invalid value"
		);
	}

	void fileParserTest() {
		auto                  initial_path = std::filesystem::current_path();
		std::filesystem::path path         = __FILE__;
		path.remove_filename();
		std::filesystem::current_path(path);

		ASSERT_EQUAL(
			"awesome_content\n",
			parseFile("test_file.txt", std::regex(".*\\.txt")).getContent().view().stdString()
		);

		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseFile("test_file.txt", std::regex(".*\\.cpp")); },
			"Regex should make it invalid"
		);

		std::filesystem::current_path(initial_path);
	}

	void filePathParserTest() {
		std::filesystem::path path = __FILE__;
		path.remove_filename();
		std::string path_str = path.string();

		ASSERT_EQUAL(
			"awesome_content\n",
			fs::File(parseFilePath(path_str + "test_file.txt", std::regex(".*\\.txt")))
				.getContent()
				.view()
				.stdString()
		);

		parseFilePath(path_str + "no_file.txt", std::regex(".*\\.txt"));

		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseFilePath(path_str + "test_file.txt", std::regex(".*\\.cpp")); },
			"Regex should make it invalid"
		);
	}
};

TESTER_COMMON_MAIN("/src/common/clah/tests/");
