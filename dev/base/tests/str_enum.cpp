#include <tester/tester.hpp>
#include <base/stringifyable_enum.hpp>

MAKE_STRINGIFYABLE_ENUM(n, u64, ZeroElements);
MAKE_STRINGIFYABLE_ENUM(n, u64, OneElement, A);
MAKE_STRINGIFYABLE_ENUM(n, u64, TwoElements, A, B);
MAKE_STRINGIFYABLE_ENUM(n, u64, ThreeElements, A, B, C);

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
	void badConversionTest() {
		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::ZeroElements>(base::StrID("A")); },
			"Bad conversion from string to enum was valid"
		);

		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::OneElement>(base::StrID("B")); },
			"Bad conversion from string to enum was valid"
		);

		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::TwoElements>(base::StrID("C")); },
			"Bad conversion from string to enum was valid"
		);

		assertThrows<base::Panic>(
			[]() { base::strToEnum<n::ThreeElements>(base::StrID("D")); },
			"Bad conversion from string to enum was valid"
		);
	}

	void goodConversionTest() {
		ASSERT_EQUAL(base::strToEnum<n::OneElement>(base::StrID("A")), n::OneElement::A);

		ASSERT_EQUAL(base::strToEnum<n::TwoElements>(base::StrID("A")), n::TwoElements::A);
		ASSERT_EQUAL(base::strToEnum<n::TwoElements>(base::StrID("B")), n::TwoElements::B);

		ASSERT_EQUAL(base::strToEnum<n::ThreeElements>(base::StrID("A")), n::ThreeElements::A);
		ASSERT_EQUAL(base::strToEnum<n::ThreeElements>(base::StrID("B")), n::ThreeElements::B);
		ASSERT_EQUAL(base::strToEnum<n::ThreeElements>(base::StrID("C")), n::ThreeElements::C);

		ASSERT_EQUAL(base::enumToStr(n::OneElement::A), base::StrID("A"));

		ASSERT_EQUAL(base::enumToStr(n::TwoElements::A), base::StrID("A"));
		ASSERT_EQUAL(base::enumToStr(n::TwoElements::B), base::StrID("B"));

		ASSERT_EQUAL(base::enumToStr(n::ThreeElements::A), base::StrID("A"));
		ASSERT_EQUAL(base::enumToStr(n::ThreeElements::B), base::StrID("B"));
		ASSERT_EQUAL(base::enumToStr(n::ThreeElements::C), base::StrID("C"));
	}

	void countTest() {
		ASSERT_EQUAL(n::ZeroElements::COUNT, 0);
		ASSERT_EQUAL(n::OneElement::COUNT, 1);
		ASSERT_EQUAL(n::TwoElements::COUNT, 2);
		ASSERT_EQUAL(n::ThreeElements::COUNT, 3);
	}
};

TESTER_COMMON_MAIN("/base/tests/");
