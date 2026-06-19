#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <token_source/source.hpp>

#include <array>
#include <sstream>
#include <utility>

class LexerErrorTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LexerErrorTests

	struct GenExample;
	static std::vector<GenExample*> examples;

	struct GenExample {
		std::string code;

		GenExample(std::string code): code(std::move(code)) { examples.push_back(this); }

		virtual bool operator()() = 0;
		[[nodiscard]]
		virtual std::string message() const
			= 0;

		virtual ~GenExample() = default;
	};

	template<bool good = true>
	struct Example: public GenExample {
		Example(std::string code): GenExample(std::move(code)) {}

		bool operator()() override {
			auto file = tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(code));
			return file->tokenize() == good;
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while lexing: `" << code << "`";
			ss << " expected parsing to " << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<bool good>
	void testExample(Example<good>& example) {
		assertTrue(example(), example.message());
	}

	std::array<std::unique_ptr<GenExample>, 6> unclosed_eof = {
		std::make_unique<Example<false>>("("), std::make_unique<Example<false>>("{"),
		std::make_unique<Example<false>>("["), std::make_unique<Example<false>>("\""),
		std::make_unique<Example<false>>("'"), std::make_unique<Example<false>>("#{"),
	};

	std::array<std::unique_ptr<GenExample>, 2> unclosed_eol = {
		std::make_unique<Example<false>>("\"\n"),
		std::make_unique<Example<false>>("'\n"),
	};

	// Literals ending in a backslash right before EOF must produce an
	// unclosed-literal diagnostic instead of crashing the lexer.
	std::array<std::unique_ptr<GenExample>, 3> trailing_backslash_eof = {
		std::make_unique<Example<false>>(R"("abc\)"),
		std::make_unique<Example<false>>(R"('a\)"),
		std::make_unique<Example<false>>(R"(f"abc\)"),
	};

	std::array<std::unique_ptr<GenExample>, 2> bad_char_count = {
		std::make_unique<Example<false>>("''"),
		std::make_unique<Example<false>>("'aaaa'"),
	};

	std::array<std::unique_ptr<GenExample>, 4> good_char = {
		std::make_unique<Example<true>>("'a'"),
		std::make_unique<Example<true>>("'z'"),
		std::make_unique<Example<true>>("'+'"),
		std::make_unique<Example<true>>("'\\n'"),
	};

	std::array<std::unique_ptr<GenExample>, 4> bad_fmt_str = {
		std::make_unique<Example<false>>(R"(f")"),
		std::make_unique<Example<false>>(R"(f"\n)"),
		std::make_unique<Example<false>>(R"(f"{")"),
		std::make_unique<Example<false>>(R"(f"{}x)"),
	};

	std::array<std::unique_ptr<GenExample>, 1> bad_type_specifier
		= { std::make_unique<Example<false>>("123abc") };

	Example<false> bad_char_start{ "\xCC\x80" };

	void exampleTests() {
		for (auto e: examples) assertTrue((*e)(), e->message());
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(exampleTests); }

public:
	~LexerErrorTests() override = default;
};

std::vector<LexerErrorTests::GenExample*> LexerErrorTests::examples = {};

TESTER_COMMON_MAIN("/src/compiler/tokenizer/lexer/tests");
