#include <version/version.hpp>

#include <tester/tester.hpp>

#include <algorithm>
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

	bool isLowerHexDigit(char c) { return (c >= '0' and c <= '9') or (c >= 'a' and c <= 'f'); }

}  // namespace

class VersionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VersionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(renderShortNamesTheToolTest);
		TESTER_ADD_TEST(commitHashIsHexTest);
		TESTER_ADD_TEST(renderShortShowsCommitHashTest);
		TESTER_ADD_TEST(renderVerboseShowsCommitHashRowTest);
		TESTER_ADD_TEST(renderVerboseNamesTheToolAndAddsExtraTest);
		TESTER_ADD_TEST(renderVerboseSkipsEmptyExtraTest);
	}

private:
	void renderShortNamesTheToolTest() {
		ASSERT_TRUE(version::renderShort(TOOL_NAME).starts_with(std::string(TOOL_NAME) + " "));
	}

	// Outside a git worktree there is no commit to name, so the hash is empty and these tests have
	// nothing to check.
	void commitHashIsHexTest() {
		const std::string_view hash = version::commitHash();
		ASSERT_TRUE(std::ranges::all_of(hash, isLowerHexDigit));
	}

	void renderShortShowsCommitHashTest() {
		if (version::commitHash().empty()) return;
		ASSERT_TRUE(
			contains(version::renderShort(TOOL_NAME), "(" + std::string(version::commitHash()))
		);
	}

	void renderVerboseShowsCommitHashRowTest() {
		if (version::commitHash().empty()) return;
		const std::string block = version::renderVerbose(TOOL_NAME);
		ASSERT_TRUE(hasRow(block, "commit-hash"));
		ASSERT_TRUE(contains(block, version::commitHash()));
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
