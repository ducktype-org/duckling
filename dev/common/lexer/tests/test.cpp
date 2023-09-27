#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <tester/tester.hpp>

class SimpleLexerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleLexerTest

	std::optional<lexer::TokenData> td;

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Lexer Test") {
		lexer::init();
		rift_def::setKeywordMode(rift_def::KeywordMode::RiftSource);

		fs::FilePath file(path("token_code.rift"));
		td = lexer::tokenizeFile(file);
		TESTER_ADD_TEST(testBasicStructure);
		TESTER_ADD_TEST(testGroup0);
		TESTER_ADD_TEST(testGroup1);
		TESTER_ADD_TEST(testGroup2);
		TESTER_ADD_TEST(testGroup3);
		TESTER_ADD_TEST(testGroup4);
		TESTER_ADD_TEST(testGroup5);
		TESTER_ADD_TEST(testGroup6);
		TESTER_ADD_TEST(testGroup7);
		TESTER_ADD_TEST(testSourcePosition);
	}

	~SimpleLexerTest() override = default;

private:


	constexpr static std::array<std::string_view, 8> group_names = {
		"",
		"keyword",
		"operator",
		"identifier",
		"special",
		"comment",
		"numLiteral",
		"string",
	};

	void testBasicStructure() {
		assert(td->tokens.size() == 8, "Wrong amount of top-level token groups");
	}

	void checkGroupIsGroup(usize index) {
		assert(td->tokens[index].isGroup(), "Group is not a group");
	}

	void testGroup0() {
		checkGroupIsGroup(0);
		const auto& inner_tokens = td->tokens[0].getRecursive();

		assert(inner_tokens.size() == 3,
		       "Expected 3 tokens, got " + std::to_string(inner_tokens.size()));

		assert(inner_tokens[0].getType() == lexer::Token::Type::RoundGroup,
		       "First group is not RoundGroup");

		assert(inner_tokens[1].getType() == lexer::Token::Type::SquareGroup,
		       "Second group is not SquareGroup");

		assert(inner_tokens[2].getType() == lexer::Token::Type::CurlyGroup,
		       "Second group is not CurlyGroup");

		for (usize i = 0; i < 3; i++) {
			assert(inner_tokens[i].getRecursive().empty(),
			       "Group " + std::to_string(i) + " is not empty");
		}
	}

	template<usize index, lexer::Token::Type token_type, bool (lexer::Token::*isTokenType)() const>
	void testTokenGroup() {
		checkGroupIsGroup(index);
		auto& inner_tokens = td->tokens[index].getRecursive();
		message("got " + std::to_string(inner_tokens.size()) + " tokens");
		for (const auto& token: inner_tokens) {
			assert(token.getType() == token_type,
			       std::string("Type of token `") + std::string(token.getStrValue()) +
			           "` is not a " + std::string(group_names[index]),
			       false);
			assert((token.*isTokenType)(),
			       std::string("Token `") + std::string(token.getStrValue()) + "` is not a " +
			           std::string(group_names[index]),
			       false);
		}
	}

	void testGroup1() {
		testTokenGroup<1, lexer::Token::Type::Keyword, &lexer::Token::isKeyword>();
	}

	void testGroup2() {
		testTokenGroup<2, lexer::Token::Type::Identifier, &lexer::Token::isIdentifier>();
	}

	void testGroup3() {
		testTokenGroup<3, lexer::Token::Type::Operator, &lexer::Token::isOperator>();
	}

	void testGroup4() {
		testTokenGroup<4, lexer::Token::Type::Special, &lexer::Token::isSpecial>();
	}

	void testGroup5() {
		testTokenGroup<5, lexer::Token::Type::Comment, &lexer::Token::isComment>();
	}

	void testGroup6() {
		testTokenGroup<6, lexer::Token::Type::NumLiteral, &lexer::Token::isNumLiteral>();
	}

	void testGroup7() { testTokenGroup<7, lexer::Token::Type::String, &lexer::Token::isString>(); }

	void testSourcePosition() {
		const auto& position = td->tokens[1].getRecursive().front().getPosition();
		assert(position.getLineNumber() == 5, "Wrong line number");
		assert(position.getColumn() == 2, "Wrong column");
		assert(position.getStart() == 15, "Wrong start index");
		assert(position.getEnd() == 19, "Wrong end index");
		assert(position.getSourceChars() == "while", "Wrong getSourceChars()");
	}
};


TESTER_COMMON_MAIN("/common/lexer/tests/");
