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
		TESTER_ADD_TEST(stringListParserTest);
	}

private:
	static i64 parseInt(const std::string& str) {
		return std::any_cast<i64>(clah::IntParser::make()->parse(str).value);
	}

	static std::string parseString(const std::string& str) {
		return std::any_cast<std::string>(clah::StringParser::make()->parse(str).value);
	}

	using Range = std::pair<i64, i64>;

	static Range parseRange(const std::string& str) {
		auto val
			= std::any_cast<clah::RangeParser::Range>(clah::RangeParser::make()->parse(str).value);
		return { val.begin, val.end };
	}

	static std::vector<std::string> parseStringList(const std::string& str) {
		return std::any_cast<std::vector<std::string>>(
			clah::StringListParser::make(clah::StringParser::make())->parse(str).value
		);
	}

	static fs::File parseFile(const std::string& str, const std::regex& regex = std::regex(".*")) {
		return std::any_cast<fs::File>(clah::FileParser::make(regex)->parse(str).value);
	}

	static fs::FilePath parseFilePath(
		const std::string& str, const std::regex& regex = std::regex(".*")
	) {
		return std::any_cast<fs::FilePath>(clah::FilePathParser::make(regex)->parse(str).value);
	}

	void intParserTest() {
		ASSERT_EQUAL(123, parseInt("123"));
		ASSERT_EQUAL(-9, parseInt("-9"));
		ASSERT_EQUAL(0, parseInt("-0"));

		auto parsed2 = clah::IntParser::make()->parse({ "123" });
		ASSERT_EQUAL("123", parsed2.raw_source);

		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseInt("str"); }, "Cannot parse str to int"
		);
		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseInt("123 123"); }, "Cannot parse 123 123 to int"
		);
		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseInt("123z"); }, "Cannot parse 123z to int"
		);
	}

	void stringParserTest() { ASSERT_EQUAL("str", parseString("str")); }

	void rangeParserTest() {
		ASSERT_EQUAL(Range(1, 2), parseRange("1..2"));
		ASSERT_EQUAL(Range(-1, 2), parseRange("-1..2"));
		ASSERT_EQUAL(Range(5, 2), parseRange("5..2"));
		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseRange("1 .. 3"); }, "Should throw on invalid value"
		);
	}

	static fs::File makeParsedFile() {
		return fs::FileManager::createRandomTempDirectory().createSubFile(
			"awesome_content\n", "test_file.txt"
		);
	}

	void fileParserTest() {
		const auto path = makeParsedFile().getFilePath().native();

		ASSERT_EQUAL(
			"awesome_content\n",
			parseFile(path, std::regex(".*\\.txt")).getContent().view().stdString()
		);

		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseFile(path, std::regex(".*\\.cpp")); }, "Regex should make it invalid"
		);
	}

	void filePathParserTest() {
		const auto path = makeParsedFile().getFilePath().native();

		ASSERT_EQUAL(
			"awesome_content\n",
			fs::File(parseFilePath(path, std::regex(".*\\.txt"))).getContent().view().stdString()
		);

		// The parser accepts a path that does not exist yet.
		const auto missing = (std::filesystem::path(path).parent_path() / "no_file.txt").string();
		parseFilePath(missing, std::regex(".*\\.txt"));

		assertThrows<clah::exceptions::ValueParsingException>(
			[&]() { parseFilePath(path, std::regex(".*\\.cpp")); }, "Regex should make it invalid"
		);
	}

	void stringListParserTest() {
		ASSERT_EQUAL(
			(std::vector<std::string>{ "str1", "str2", "str3" }), parseStringList("str1,str2,str3")
		);

		ASSERT_EQUAL(
			(std::vector<std::string>{ "str1", "str2", "str3" }), parseStringList("str1, str2, str3")
		);

		ASSERT_EQUAL(
			(std::vector<std::string>{ "str1", "str2", "str3" }),
			parseStringList("str1 ,   str2 ,   str3   ")
		);

		ASSERT_EQUAL((std::vector<std::string>{}), parseStringList(""));

		ASSERT_EQUAL((std::vector<std::string>{ "str1" }), parseStringList("str1"));
	}
};

TESTER_COMMON_MAIN("/src/common/clah/tests/");
