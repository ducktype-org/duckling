// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <version/version.hpp>

#include <tester/tester.hpp>

#include <array>
#include <string>
#include <string_view>

namespace {

	constexpr std::string_view TOOL_NAME{ "test_duck_test_duck" };

	bool contains(std::string_view block, std::string_view text) {
		return block.find(text) != std::string_view::npos;
	}

	bool hasRow(std::string_view block, std::string_view label) {
		return contains(block, "\n" + std::string(label) + ":");
	}

}  // namespace

class VersionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VersionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(renderShortNamesTheToolTest);
		TESTER_ADD_TEST(renderVerboseNamesTheToolAndAddsExtraTest);
		TESTER_ADD_TEST(renderVerboseSkipsEmptyExtraTest);
	}

private:
	void renderShortNamesTheToolTest() {
		ASSERT_TRUE(version::renderShort(TOOL_NAME).starts_with(std::string(TOOL_NAME) + " "));
	}

	void renderVerboseNamesTheToolAndAddsExtraTest() {
		const std::array<version::ExtraField, 1> extra{
			version::ExtraField{ "LLVM", "19.1.7" },
		};
		const std::string block = version::renderVerbose(TOOL_NAME, extra);

		ASSERT_TRUE(block.starts_with(version::renderShort(TOOL_NAME)));
		ASSERT_TRUE(hasRow(block, "LLVM"));
		ASSERT_TRUE(contains(block, "19.1.7"));
	}

	void renderVerboseSkipsEmptyExtraTest() {
		const std::array<version::ExtraField, 1> extra{
			version::ExtraField{ "JIT", "" },
		};
		ASSERT_TRUE(not hasRow(version::renderVerbose(TOOL_NAME, extra), "JIT"));
	}
};

TESTER_COMMON_MAIN("/src/common/version/tests/");
