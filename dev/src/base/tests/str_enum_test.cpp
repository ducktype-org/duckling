#include <base/extend_cpp/stringifyable_enum.hpp>

#include <tester/tester.hpp>

MAKE_STRINGIFYABLE_ENUM(n, u64, ZeroElements);
MAKE_STRINGIFYABLE_ENUM(n, u64, OneElement, A);
MAKE_STRINGIFYABLE_ENUM(n, u64, TwoElements, A, B);
MAKE_STRINGIFYABLE_ENUM(n, u64, ThreeElements, A, B, C);

MAKE_STRINGIFYABLE_ENUM(n, i64, SingedInt, A, B);

class StrEnumTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StrEnumTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(badConversionTest);
		TESTER_ADD_TEST(goodConversionTest);
		TESTER_ADD_TEST(countTest);
	}

private:
	template<class EnumType>
	void checkThatCountDoesNotConvert() {
		assertThrows<base::Panic>(
			[]() { base::strToEnum<EnumType>("COUNT"); },
			"Bad conversion from string to enum was valid"
		);
		assertThrows<base::Panic>(
			[]() { base::enumToStr(EnumType::COUNT); },
			"Bad conversion from enum to string was valid"
		);
	}

	void badConversionTest() {
#if defined(BUILD_TYPE_DEV)
		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::ZeroElements>("A"); },
			"Bad conversion from string to enum was valid"
		);

		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::OneElement>("B"); },
			"Bad conversion from string to enum was valid"
		);

		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::TwoElements>("C"); },
			"Bad conversion from string to enum was valid"
		);

		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::ThreeElements>("D"); },
			"Bad conversion from string to enum was valid"
		);

		checkThatCountDoesNotConvert<n::ZeroElements>();
		checkThatCountDoesNotConvert<n::OneElement>();
		checkThatCountDoesNotConvert<n::TwoElements>();
		checkThatCountDoesNotConvert<n::ThreeElements>();
		checkThatCountDoesNotConvert<n::SingedInt>();
#endif
	}

	void goodConversionTest() {
		ASSERT_EQUAL(base::strToEnum<n::OneElement>("A"), n::OneElement::A);

		ASSERT_EQUAL(base::strToEnum<n::TwoElements>("A"), n::TwoElements::A);
		ASSERT_EQUAL(base::strToEnum<n::TwoElements>("B"), n::TwoElements::B);

		ASSERT_EQUAL(base::strToEnum<n::ThreeElements>("A"), n::ThreeElements::A);
		ASSERT_EQUAL(base::strToEnum<n::ThreeElements>("B"), n::ThreeElements::B);
		ASSERT_EQUAL(base::strToEnum<n::ThreeElements>("C"), n::ThreeElements::C);

		ASSERT_EQUAL(base::strToEnum<n::SingedInt>("A"), n::SingedInt::A);
		ASSERT_EQUAL(base::strToEnum<n::SingedInt>("B"), n::SingedInt::B);

		ASSERT_EQUAL(base::enumToStr(n::OneElement::A), "A");

		ASSERT_EQUAL(base::enumToStr(n::TwoElements::A), "A");
		ASSERT_EQUAL(base::enumToStr(n::TwoElements::B), "B");

		ASSERT_EQUAL(base::enumToStr(n::ThreeElements::A), "A");
		ASSERT_EQUAL(base::enumToStr(n::ThreeElements::B), "B");
		ASSERT_EQUAL(base::enumToStr(n::ThreeElements::C), "C");

		ASSERT_EQUAL(base::enumToStr(n::SingedInt::A), "A");
		ASSERT_EQUAL(base::enumToStr(n::SingedInt::B), "B");
	}

	void countTest() {
		ASSERT_EQUAL(std::to_underlying(n::ZeroElements::COUNT), 0);
		ASSERT_EQUAL(std::to_underlying(n::OneElement::COUNT), 1);
		ASSERT_EQUAL(std::to_underlying(n::TwoElements::COUNT), 2);
		ASSERT_EQUAL(std::to_underlying(n::ThreeElements::COUNT), 3);
		ASSERT_EQUAL(std::to_underlying(n::SingedInt::COUNT), 2);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
