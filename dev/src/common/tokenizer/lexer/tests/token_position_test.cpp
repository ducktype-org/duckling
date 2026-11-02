// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <token_source/source.hpp>

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

	MBox<tokenizer::TokenSource> td;

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simplePositionTest);
		TESTER_ADD_TEST(positionCompTest);
	}

private:
	void simplePositionTest() {
		fs::File file(path("fun.duck"));
		td = tokenizer::makeTokenSource(file);
		td->tokenize();

		std::stringstream result_stream;
		print(td->getTokenData().tokens, result_stream);

		auto corr_json = fs::File(path("fun_position.json")).getContent();
		auto corr      = corr_json.view().stringView();

		assertTrue(testing_utils::compareJson(result_stream.str(), corr), "outputs are not equal");
	}

	void positionCompTest() {
		using namespace std::views;
		auto content = repeat(' ') | take(20) | std::ranges::to<std::string>();
		auto file1   = fs::FileManager::createRandomVirtualFile(content);
		auto file2   = fs::FileManager::createRandomVirtualFile(content);

		auto td1 = tokenizer::makeTokenSource(file1);
		td1->tokenize();
		auto td2 = tokenizer::makeTokenSource(file2);
		td2->tokenize();

		auto loc1 = td1->getLocation();
		auto loc2 = td2->getLocation();

		auto pos11 = dia::SourcePosition(loc1, 0, 5);
		auto pos12 = dia::SourcePosition(loc1, 0, 7);
		auto pos13 = dia::SourcePosition(loc1, 20, 20);

		auto pos21 = dia::SourcePosition(loc2, 0, 7);
		auto pos22 = dia::SourcePosition(loc2, 5, 5);

		ASSERT_TRUE(pos11 < pos12);
		ASSERT_TRUE(pos12 < pos13);
		ASSERT_TRUE(pos21 < pos22);

		ASSERT_TRUE(pos11 == pos11);
		ASSERT_TRUE(pos13 == pos13);

		ASSERT_TRUE((pos13 <=> pos21) == (pos11 <=> pos22));
	}

public:
	~LexerPositionTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/tokenizer/lexer/tests/");
