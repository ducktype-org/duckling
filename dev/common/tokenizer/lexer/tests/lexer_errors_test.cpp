#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <token_file/file.hpp>

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
			auto file = tokenizer::makeTokenFile(fs::FilePath::createVirtualFile(code));
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

TESTER_COMMON_MAIN("/compiler/tokenizer/lexer/tests");
