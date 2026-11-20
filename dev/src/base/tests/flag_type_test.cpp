// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/flag.hpp>

#include <tester/tester.hpp>

MAKE_FLAG_TYPE(test_flag_namespace, TestFlagOpts, TestFlag,
	Opt1,
	Opt2,
	Opt3,
	Opt4,
	Opt5
)

class FlagTypeTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FlagTypeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		using namespace test_flag_namespace;
		using enum TestFlagOpts;

		TestFlag empty_flag;
		ASSERT_EQUAL(empty_flag, TestFlag());
		ASSERT_TRUE(!empty_flag.contains(Opt1));

		auto flag_123 = Opt1 | Opt2 | Opt3;

		TestFlag flag_123_oth;
		flag_123_oth |= Opt1;
		flag_123_oth |= Opt2;
		flag_123_oth |= Opt3;

		ASSERT_EQUAL(flag_123_oth, flag_123);

		ASSERT_TRUE(flag_123.contains(Opt1));
		ASSERT_TRUE(flag_123.contains(Opt2));
		ASSERT_TRUE(flag_123.contains(Opt3));

		ASSERT_TRUE(flag_123.contains(Opt1 | Opt2));
		ASSERT_TRUE(flag_123.contains(Opt1 | Opt3));
		ASSERT_TRUE(flag_123.contains(Opt2 | Opt3));

		ASSERT_TRUE(!flag_123.contains(Opt4));
		ASSERT_TRUE(!flag_123.contains(Opt5));
		ASSERT_TRUE(!flag_123.contains(Opt1 | Opt4));

		auto flag_234 = Opt2 | Opt3 | Opt4;
		auto flag_23  = flag_123;
		flag_23 &= flag_234;

		ASSERT_TRUE(flag_23.contains(Opt2 | Opt3));
		ASSERT_TRUE((flag_23 & (Opt1 | Opt4 | Opt5)) == TestFlag());

		flag_23 -= Opt2 | Opt3;
		ASSERT_TRUE(not flag_23.contains(Opt2));
		ASSERT_TRUE(not flag_23.contains(Opt3));
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
