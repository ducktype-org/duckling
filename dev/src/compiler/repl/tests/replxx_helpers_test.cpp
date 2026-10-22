// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <repl/frontend_implementations/replxx_helpers.hpp>

#include <tester/tester.hpp>

#include <string>
#include <vector>

using compiler::repl::replxx_helpers::computeBraceIndentDepth;
using compiler::repl::replxx_helpers::extractWordEndingAt;
using compiler::repl::replxx_helpers::mapUtf8CodePoints;
using compiler::repl::replxx_helpers::tokenizeIdentifiers;

class ReplxxHelpersTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ReplxxHelpersTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testExtractWordEndingAt);
		TESTER_ADD_TEST(testTokenizeIdentifiers);
		TESTER_ADD_TEST(testComputeBraceIndentDepth);
		TESTER_ADD_TEST(testMapUtf8CodePoints);
	}

private:
	void testExtractWordEndingAt() {
		ASSERT_EQUAL("", extractWordEndingAt("", 0));
		ASSERT_EQUAL("foo", extractWordEndingAt("foo", 3));
		ASSERT_EQUAL("foo_bar", extractWordEndingAt("foo_bar", 7));
		ASSERT_EQUAL("foo", extractWordEndingAt("foo bar", 3));
		ASSERT_EQUAL("", extractWordEndingAt("foo bar", 0));
	}

	void testTokenizeIdentifiers() {
		std::string input  = "foo bar_1 2baz _qux";
		auto        tokens = tokenizeIdentifiers(input);
		ASSERT_EQUAL(4UL, tokens.size());
		ASSERT_EQUAL("foo", tokens[0]);
		ASSERT_EQUAL("bar_1", tokens[1]);
		ASSERT_EQUAL("2baz", tokens[2]);
		ASSERT_EQUAL("_qux", tokens[3]);
	}

	void testComputeBraceIndentDepth() {
		{
			std::string input = "{\n  {\n  }\n";
			ASSERT_EQUAL(1, computeBraceIndentDepth(input, input.size()));
		}
		{
			std::string input = "{ \"{\" }";
			ASSERT_EQUAL(0, computeBraceIndentDepth(input, input.size()));
		}
		{
			std::string input = "{ # {\n }";
			ASSERT_EQUAL(0, computeBraceIndentDepth(input, input.size()));
		}
		{
			std::string input = "{ #{ { } #} }";
			ASSERT_EQUAL(0, computeBraceIndentDepth(input, input.size()));
		}
	}

	void testMapUtf8CodePoints() {
		ASSERT_EQUAL("abc", mapUtf8CodePoints("abc"));

		{
			std::string input = std::string(
				"a\xC3"
				"\xA9"
				"b"
			);
			auto mapped = mapUtf8CodePoints(input, '*');
			ASSERT_EQUAL("a*b", mapped);
			ASSERT_EQUAL(3UL, mapped.size());
		}
		{
			std::string input  = std::string("\xF0\x9F\x98\x80");
			auto        mapped = mapUtf8CodePoints(input, '_');
			ASSERT_EQUAL("_", mapped);
			ASSERT_EQUAL(1UL, mapped.size());
		}
	}

public:
	~ReplxxHelpersTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/repl/tests/");
