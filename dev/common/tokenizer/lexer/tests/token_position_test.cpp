#include <filesystem/file.hpp>
#include <token_file/file.hpp>
#include <lexer/lexer.hpp>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

void print(const lexer::Tokens& tokens, std::ostream& out, const std::string& indent = "") {
	out << "[";
	for (const auto& token: tokens) {
		dia::SourcePosition position = token.getPosition();

		out << indent;

		out << "{\"value\" : ";
		if (token.getValue().isGood())
			out << "\"" << token.getStrValue() << "\", ";
		else
			out << "\"<EMPTY>\", ";

		auto [line, column] = position.getStartLineColumn();
		out << R"("line": ")" << line << "\",";
		out << R"("column": ")" << column << "\",";
		out << R"("raw_start": ")" << position.getStart() << "\",";
		out << R"("raw_end": ")" << position.getEnd() << "\",";

		out << R"("recursive": )";
		print(token.getRecursive(), out, indent + "	");
		out << "}, ";
	}
	out << "]";
}

class LexerPositionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LexerPositionTest

	tokenizer::OwnFile td;

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Lexer: Token Position Tests") {
		lexer::init();
		TESTER_ADD_TEST(simplePositionTest);
	}

private:
	void simplePositionTest() {
		fs::FilePath file(path("fun.rift"));
		td = lexer::tokenizeFile(file);

		std::stringstream result_stream;
		print(td->getTokenData().tokens, result_stream);
		std::cout << result_stream.str() << std::endl;

		auto corr_json = fs::getSimpleFileContent(path("fun_position.json"));
		auto corr      = corr_json.view().stringView();

		assert(testing_utils::compareJson(result_stream.str(), corr), "outputs are not equal");
	}

public:
	~LexerPositionTest() override = default;
};

TESTER_COMMON_MAIN("/common/tokenizer/lexer/tests/");
