#include <filesystem/file.hpp>
#include <tester/tester.hpp>
#include <token_source/source.hpp>

std::byte operator""_BT(unsigned long long x) {
	CORE_ASSERT(x < 256, "bad std::byte literal operator");
	return std::byte(x);
}

std::byte operator""_BT(char x) {
	CORE_ASSERT(x >= 0, "bad std::byte literal operator");
	return std::byte(x);
}

class DecodeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DecodeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(badContinuations);
		TESTER_ADD_TEST(invalidFirstBytes);
		TESTER_ADD_TEST(nonContinuation);
		TESTER_ADD_TEST(eofContinuation);
		TESTER_ADD_TEST(badUnicode);
		TESTER_ADD_TEST(badAscii);
		TESTER_ADD_TEST(goodAscii);
	}

	~DecodeTest() override = default;

protected:
	void beforeAll() override { lang_def::setKeywordMode(lang_def::KeywordMode::DucklingSource); }

private:
	template<fs::Encoding encoding = fs::Encoding::UTF8>
	void assumeBadDecode(base::RawView view) {
		std::string content{ view.stringView() };
		auto        path = fs::FileManager::createRandomVirtualFile(content);
		auto        file = tokenizer::makeTokenSource(path);
		file->decode<encoding>();
		assertTrue(file->getIntLogger()->hasErrors(), "Encoding error not found");
	}

	void badContinuations() {
		std::vector<std::vector<std::byte>> ins = { { 0b10000000_BT },
			                                        { 0b10111111_BT },
			                                        { ' '_BT, ' '_BT, 0b10101010_BT },
			                                        { 0b10111111_BT, 'A'_BT } };
		for (auto& in: ins) assumeBadDecode(base::RawView(in.data(), in.size()));
	}

	void invalidFirstBytes() {
		std::vector<std::vector<std::byte>> ins = { { 0b11111000_BT },
			                                        { 0b11111011_BT },
			                                        { ' '_BT, ' '_BT, 0b11111010_BT },
			                                        { 0b11111001_BT, 'A'_BT } };
		for (auto& in: ins) assumeBadDecode(base::RawView(in.data(), in.size()));
	}

	void nonContinuation() {
		std::vector<std::vector<std::byte>> ins = {
			{ 0b11000000_BT, ' '_BT },
			{ 0b11100111_BT, 'x'_BT },
			{ 0b11100111_BT, 0b10111111_BT, 'u'_BT },
			{ 0b11110111_BT, 0b10000000_BT, 0b10000000_BT, 'z'_BT },
		};
		for (auto& in: ins) assumeBadDecode(base::RawView(in.data(), in.size()));
	}

	void eofContinuation() {
		std::vector<std::vector<std::byte>> ins = {
			{ 0b11000000_BT },
			{ 0b11100111_BT, 0b10111111_BT },
			{ ' '_BT, 0b11110111_BT, 0b10000000_BT, 0b10000000_BT },
			{ 'x'_BT, 0b11110111_BT, 0b10000000_BT },
			{ 0b11110111_BT },
		};
		for (auto& in: ins) assumeBadDecode(base::RawView(in.data(), in.size()));
	}

	void badUnicode() {
		std::vector<std::vector<std::byte>> ins = {
			{ 0b11110111_BT, 0b10111111_BT, 0b10111111_BT, 0b10111111_BT },
			{ ' '_BT, 'x'_BT, 0b11101101_BT, 0b10100000_BT, 0b10000000_BT },
		};
		for (auto& in: ins) assumeBadDecode(base::RawView(in.data(), in.size()));
	}

	void badAscii() {
		std::vector<std::vector<std::byte>> ins = {
			{ 0b11110111_BT },
			{ ' '_BT, 0b10000000_BT, 'x'_BT },
		};
		for (auto& in: ins)
			assumeBadDecode<fs::Encoding::UsAscii>(base::RawView(in.data(), in.size()));
	}

	void goodAscii() {
		std::vector<std::byte> in;
		in.reserve(128);
		for (uchar c = 0; c < 128; c++) in.push_back(std::byte{ c });

		std::string content{ reinterpret_cast<char*>(in.data()), in.size() };
		auto        path = fs::FileManager::createRandomTempFile(content);
		auto        file = tokenizer::makeTokenSource(path);
		file->decode<fs::Encoding::UsAscii>();
		assertTrue(file->getIntLogger()->good(), "Valid Ascii not accepted");
	}
};

TESTER_COMMON_MAIN("/src/common/tokenizer/lexer/tests/");
