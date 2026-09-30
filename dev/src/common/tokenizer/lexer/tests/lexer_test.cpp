#include <filesystem/file.hpp>
#include <lexer/token.hpp>
#include <tester/tester.hpp>
#include <token_source/source.hpp>

#include <string>
#include <string_view>

class SimpleLexerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleLexerTest

	MBox<tokenizer::TokenSource> td;

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBasicStructure);
		TESTER_ADD_TEST(testGroup0);
		TESTER_ADD_TEST(testGroup1);
		TESTER_ADD_TEST(testGroup2);
		TESTER_ADD_TEST(testGroup3);
		TESTER_ADD_TEST(testGroup4);
		TESTER_ADD_TEST(testGroup5);
		TESTER_ADD_TEST(testGroup6);
		TESTER_ADD_TEST(testGroup7);
		TESTER_ADD_TEST(testGroup8);
		TESTER_ADD_TEST(testGroup9);
		TESTER_ADD_TEST(testSourcePosition);
		TESTER_ADD_TEST(testLiteralTextIsNotKeyword);
		TESTER_ADD_TEST(testDescribeSentinelsDoNotLeakInternalNames);
		TESTER_ADD_TEST(testDescribeWording);
	}

	~SimpleLexerTest() override = default;

protected:
	void beforeAll() override {
		lang_def::setKeywordMode(lang_def::KeywordMode::DucklingSource);

		fs::File file(path("token_code.duck"));
		td = tokenizer::makeTokenSource(file);
		td->tokenize();
	}

private:
	constexpr static std::array<std::string_view, 10> GROUP_NAMES
		= { "",        "keyword",    "operator", "identifier", "special",
		    "comment", "numLiteral", "string",   "char",       "formatstring" };

	void testBasicStructure() {
		assertTrue(td->getTokenData().tokens.size() == 10, "Wrong amount of top-level token groups");
	}

	void checkTokenIsBracketGroup(usize index) {
		assertTrue(
			td->getTokenData().tokens[index].isBracketGroup(), "Token is not a bracket group"
		);
	}

	void testGroup0() {
		checkTokenIsBracketGroup(0);
		const auto& inner_tokens = td->getTokenData().tokens[0].getRecursive();

		assertTrue(
			inner_tokens.size() == 3, "Expected 3 tokens, got " + std::to_string(inner_tokens.size())
		);

		assertTrue(
			inner_tokens[0].isBracketGroup(lexer::Token::BracketType::Round),
			"First token is not a round bracket group"
		);

		assertTrue(
			inner_tokens[1].isBracketGroup(lexer::Token::BracketType::Square),
			"Second token is not a square bracket group"
		);

		assertTrue(
			inner_tokens[2].isBracketGroup(lexer::Token::BracketType::Curly),
			"Third token is not a curly bracket group"
		);

		for (usize i = 0; i < 3; i++) {
			assertTrue(
				inner_tokens[i].getRecursive().empty(),
				"Group " + std::to_string(i) + " is not empty"
			);
		}
	}

	template<usize index, lexer::Token::Type token_type, bool (lexer::Token::*isTokenType)() const>
	void testTokenGroup() {
		checkTokenIsBracketGroup(index);
		auto& inner_tokens = td->getTokenData().tokens[index].getRecursive();
		message("got " + std::to_string(inner_tokens.size()) + " tokens");
		for (const auto& token: inner_tokens) {
			assertTrue(
				token.getType() == token_type,
				std::string("Type of token `") + std::string(token.getStrValue()) + "` is not a "
					+ std::string(GROUP_NAMES[index]),
				false
			);
			assertTrue(
				(token.*isTokenType)(),
				std::string("Token `") + std::string(token.getStrValue()) + "` is not a "
					+ std::string(GROUP_NAMES[index]),
				false
			);
		}
	}

	void testGroup1() {
		testTokenGroup<1, lexer::Token::Type::Keyword, &lexer::Token::isKeyword>();
	}

	void testGroup2() {
		testTokenGroup<2, lexer::Token::Type::Identifier, &lexer::Token::isIdentifier>();
	}

	void testGroup3() {
		testTokenGroup<3, lexer::Token::Type::Operator, &lexer::Token::isOperatorSymbol>();
	}

	void testGroup4() {
		testTokenGroup<4, lexer::Token::Type::Special, &lexer::Token::isSpecial>();
	}

	void testGroup5() {
		testTokenGroup<5, lexer::Token::Type::Comment, &lexer::Token::isComment>();
	}

	void testGroup6() {
		testTokenGroup<6, lexer::Token::Type::NumLiteralGroup, &lexer::Token::isNumLiteralGroup>();
	}

	void testGroup7() { testTokenGroup<7, lexer::Token::Type::String, &lexer::Token::isString>(); }

	void testGroup8() { testTokenGroup<8, lexer::Token::Type::Char, &lexer::Token::isChar>(); }

	void testGroup9() {
		testTokenGroup<9, lexer::Token::Type::FormatString, &lexer::Token::isFormatString>();
	}

	/**
	 * @brief A string or char whose text is a keyword or a special (`"match"`, `';'`) must not be
	 * recognised as one, or the parser reads `return "match";` as a `match` expression.
	 */
	void testLiteralTextIsNotKeyword() {
		for (usize group: { 7UZ, 8UZ }) {
			for (const auto& token: td->getTokenData().tokens[group].getRecursive()) {
				assertTrue(
					not token.is(lang_def::Keyword::Match) and not token.is(lang_def::Keyword::If)
						and not token.is(lang_def::Special::Semicolon),
					base::strConcat(
						"Literal `", token.getStrValue(), "` is read as a keyword/special"
					)
				);
			}
		}

		const auto& keywords = td->getTokenData().tokens[1].getRecursive();
		ASSERT_TRUE(keywords.at(3).is(lang_def::Keyword::If));
		ASSERT_TRUE(
			td->getTokenData().tokens[4].getRecursive().at(0).is(lang_def::Special::Semicolon)
		);
	}

	/**
	 * @brief `Token::describe()` is what parser diagnostics print after `but got: `.
	 * A sentinel should not be printed as such, but as a boundary description,
	 * and the ordinary tokens should keep their `Kind 'text'` rendering.
	 */
	void testDescribeSentinelsDoNotLeakInternalNames() {
		const auto pos = dia::SourcePosition::fakePosition();

		const auto check = [&](std::string_view what, const lexer::Token& token) {
			const std::string described = token.describe();

			assertTrue(
				described.find("Sentinel") == std::string::npos,
				base::strConcat(what, ": `", described, "` leaks the internal token kind")
			);
			assertTrue(
				described.find("''") == std::string::npos,
				base::strConcat(what, ": `", described, "` contains an empty quoted payload")
			);
			assertTrue(
				not described.empty(), base::strConcat(what, ": rendered an empty description")
			);
		};

		check("end of the fallback window", lexer::Token::makeIdentifier("x", pos).asSentinel());
		check("end of file", lexer::Token::makeSentinelEof(pos));
		check("beginning of file", lexer::Token::makeSentinelBof(pos));

		for (const char* bracket: { "(", ")", "[", "]", "{", "}" })
			check(
				base::strConcat("bracket `", bracket, "`"),
				lexer::Token::makeSentinel(base::RawView(bracket), pos)
			);
	}

	/**
	 * @brief Pins the exact text of every `Token::describe()` result.
	 */
	void testDescribeWording() {
		const auto pos = dia::SourcePosition::fakePosition();

		const auto check
			= [&](std::string_view what, const lexer::Token& token, std::string_view expected) {
				  const std::string described = token.describe();
				  assertTrue(
					  std::string_view(described) == expected,
					  base::strConcat(what, ": expected `", expected, "`, got `", described, "`")
				  );
			  };

		check("identifier", lexer::Token::makeIdentifier("foo", pos), "Identifier 'foo'");
		check("keyword", lexer::Token::makeKeyword("if", pos), "Keyword 'if'");
		check("operator", lexer::Token::makeOperator("=", pos), "Operator '='");
		check("special", lexer::Token::makeSpecial(";", pos), "Special ';'");
		check("string", lexer::Token::makeString("s", pos), "String 's'");
		check("char", lexer::Token::makeChar("c", pos), "Char 'c'");
		check("number literal", lexer::Token::makeNumLiteral("1", pos), "NumLiteral '1'");
		check("type specifier", lexer::Token::makeTypeSpecifier("i32", pos), "TypeSpecifier 'i32'");
		check("comment", lexer::Token::makeComment("//c", pos), "Comment '//c'");
		check(
			"format string part",
			lexer::Token::makeFormatStringSubString("fs", pos),
			"FormatStringSubString 'fs'"
		);

		check(
			"end of the fallback window",
			lexer::Token::makeIdentifier("x", pos).asSentinel(),
			"an unexpected end of the statement"
		);
		check("end of file", lexer::Token::makeSentinelEof(pos), "EOF");
		check("beginning of file", lexer::Token::makeSentinelBof(pos), "BOF");

		// Only the closing bracket reaches a message, so we test only closing brackets.
		for (const char* bracket: { ")", "]", "}" })
			check(
				base::strConcat("closing bracket `", bracket, "`"),
				lexer::Token::makeSentinel(base::RawView(bracket), pos),
				base::strConcat("the end of the '", bracket, "' group")
			);
	}

	void testSourcePosition() {
		const auto& position = td->getTokenData().tokens[1].getRecursive().front().getPosition();
		auto [line, column]  = position.getStartLineColumn();
		ASSERT_EQUAL(line, 5);
		ASSERT_EQUAL(column, 2);
		ASSERT_EQUAL(position.getStart(), 15);
		ASSERT_EQUAL(position.getEnd(), 19);
	}
};

TESTER_COMMON_MAIN("/src/common/tokenizer/lexer/tests/");
