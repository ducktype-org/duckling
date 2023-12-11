#include <filesystem/file.hpp>
#include <lexer/decode.hpp>
#include <lexer/lexer.hpp>
#include <tester/tester.hpp>

std::byte operator ""BT(unsigned long long x) {
	RIFT_ASSERT(x <  256, "bad std::byte literal operator");
	return std::byte{x};
}

std::byte operator ""BT(char x) {
	RIFT_ASSERT(x >= 0, "bad std::byte literal operator");
	return std::byte{x};
}

class DecodeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DecodeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Decode Test") {
		lexer::init();
		rift_def::setKeywordMode(rift_def::KeywordMode::RiftSource);

		TESTER_ADD_TEST(badContinuations);
		TESTER_ADD_TEST(invalidFirstBytes);
		TESTER_ADD_TEST(nonContinuation);
		TESTER_ADD_TEST(eofContinuation);
		TESTER_ADD_TEST(badUnicode);
		TESTER_ADD_TEST(badAscii);
		TESTER_ADD_TEST(goodAscii);
	}

	~DecodeTest() override = default;

private:
	template<fs::Encoding encoding = fs::Encoding::UTF8>
	void assumeBadDecode(base::RawView view) {
		dia::ErrorState state;
		lexer::decode<encoding>(view, state);
		//state.dumpLog();
		assert(state.fail(), "Encoding error not found");
	}

	void badContinuations() {
		std::vector<std::vector<std::byte>> ins = {
			{0b10000000BT},
			{0b10111111BT},
			{' 'BT, ' 'BT, 0b10101010BT},
			{0b10111111BT, 'A'BT}
		};
		for(auto& in: ins) {
			assumeBadDecode(base::RawView(in.data(), in.size()));
		}
	}

	void invalidFirstBytes() {
		std::vector<std::vector<std::byte>> ins = {
			{0b11111000BT},
			{0b11111011BT},
			{' 'BT, ' 'BT, 0b11111010BT},
			{0b11111001BT, 'A'BT}
		};
		for(auto& in: ins) {
			assumeBadDecode(base::RawView(in.data(), in.size()));
		}
	}

	void nonContinuation() {
		std::vector<std::vector<std::byte>> ins = {
			{0b11000000BT, ' 'BT},
			{0b11100111BT, 'x'BT},
			{0b11100111BT, 0b10111111BT, 'u'BT},
			{0b11110111BT, 0b10000000BT, 0b10000000BT, 'z'BT},
		};
		for(auto& in: ins) {
			assumeBadDecode(base::RawView(in.data(), in.size()));
		}
	}

	void eofContinuation() {
		std::vector<std::vector<std::byte>> ins = {
			{0b11000000BT},
			{0b11100111BT, 0b10111111BT},
			{' 'BT, 0b11110111BT, 0b10000000BT, 0b10000000BT},
			{'x'BT, 0b11110111BT, 0b10000000BT},
			{0b11110111BT},
		};
		for(auto& in: ins) {
			assumeBadDecode(base::RawView(in.data(), in.size()));
		}
	}

	void badUnicode() {
		std::vector<std::vector<std::byte>> ins = {
			{0b11110111BT, 0b10111111BT, 0b10111111BT, 0b10111111BT},
			{' 'BT, 'x'BT, 0b11101101BT, 0b10100000BT, 0b10000000BT},
		};
		for(auto& in: ins) {
			assumeBadDecode(base::RawView(in.data(), in.size()));
		}
	}
	void badAscii() {
		std::vector<std::vector<std::byte>> ins = {
			{0b11110111BT},
			{' 'BT, 0b10000000BT, 'x'BT},
		};
		for(auto& in: ins) {
			assumeBadDecode<fs::Encoding::US_ASCII>(base::RawView(in.data(), in.size()));
		}
	}

	void goodAscii() {
		std::vector<std::byte> in = {};
		for(uchar c = 0; c < 128; c++) {
			in.push_back(std::byte{c});
		}
		dia::ErrorState err;
		auto res = lexer::decode<fs::Encoding::US_ASCII>(base::RawView(in.data(), in.size()), err);
		assert(err.good(), "Valid Ascii not accepted");
	}
};

TESTER_COMMON_MAIN("/common/tokenizer/lexer/tests/");