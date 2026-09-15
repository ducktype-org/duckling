#include <version/version.hpp>

#include <base/types/ints.hpp>

#include <tester/tester.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace {

	/**
	 * @brief Splits a rendered block into its lines, so a single row can be inspected.
	 *
	 * The views point into @p block, so the caller has to keep the rendered string alive - never
	 * pass a `renderVerbose(...)` temporary straight in.
	 */
	std::vector<std::string_view> linesOf(std::string_view block) {
		std::vector<std::string_view> lines;
		while (true) {
			const usize end = block.find('\n');
			if (end == std::string_view::npos) {
				lines.push_back(block);
				return lines;
			}
			lines.push_back(block.substr(0, end));
			block.remove_prefix(end + 1);
		}
	}

	/** @brief The column a `label: value` row puts its value in, i.e. where the padding ends. */
	usize valueColumnOf(std::string_view row) {
		const usize colon = row.find(':');
		if (colon == std::string_view::npos) return std::string_view::npos;
		return row.find_first_not_of(' ', colon + 1);
	}

	bool isAllDigits(std::string_view value) {
		return not value.empty() && value.find_first_not_of("0123456789") == std::string_view::npos;
	}

}  // namespace

// Every value this module reports comes from the build configuration, so the test cannot know what
// any of them is. What it can check is that they agree with each other and that the two renderers
// lay them out the way the binaries promise.
class VersionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VersionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(semverIsComposedOfItsComponentsTest);
		TESTER_ADD_TEST(prereleaseFlagFollowsSuffixTest);
		TESTER_ADD_TEST(buildFactsArePresentTest);
		TESTER_ADD_TEST(renderShortTest);
		TESTER_ADD_TEST(renderShortWithoutToolNameTest);
		TESTER_ADD_TEST(renderVerboseStartsWithShortTest);
		TESTER_ADD_TEST(renderVerboseSkipsEmptyValuesTest);
		TESTER_ADD_TEST(renderVerboseAppendsExtraRowsTest);
		TESTER_ADD_TEST(renderVerboseAlignsValuesTest);
	}

private:
	void semverIsComposedOfItsComponentsTest() {
		ASSERT_TRUE(isAllDigits(version::semverMajor()));
		ASSERT_TRUE(isAllDigits(version::semverMinor()));
		ASSERT_TRUE(isAllDigits(version::semverPatch()));

		std::string expected = std::string(version::semverMajor()) + "."
		                     + std::string(version::semverMinor()) + "."
		                     + std::string(version::semverPatch());
		if (not version::prerelease().empty()) expected += "-" + std::string(version::prerelease());

		ASSERT_EQUAL_PRINT(expected, std::string(version::semver()));
	}

	void prereleaseFlagFollowsSuffixTest() {
		ASSERT_EQUAL(not version::prerelease().empty(), version::isPrerelease());
	}

	void buildFactsArePresentTest() {
		// These four are always known: CMake fills them in for every configuration.
		ASSERT_TRUE(not version::buildType().empty());
		ASSERT_TRUE(not version::hostSystem().empty());
		ASSERT_TRUE(not version::hostProcessor().empty());
		ASSERT_TRUE(not version::compilerId().empty());
	}

	void renderShortTest() {
		const std::string line = version::renderShort("duckc");

		const std::string head = "duckc " + std::string(version::semver());
		ASSERT_TRUE(line.starts_with(head));

		if (version::commitDate().empty()) {
			// No git worktree: the whole parenthesised group disappears.
			ASSERT_EQUAL_PRINT(head, line);
			return;
		}

		std::string tail = " (" + std::string(version::commitDate());
		if (version::isDirty()) tail += "-dirty";
		tail += ")";
		ASSERT_EQUAL_PRINT(head + tail, line);
	}

	void renderShortWithoutToolNameTest() {
		// An empty tool name must not leave a leading space in front of the version.
		ASSERT_TRUE(version::renderShort("").starts_with(version::semver()));
	}

	void renderVerboseStartsWithShortTest() {
		const std::string block = version::renderVerbose("VM");
		const auto        lines = linesOf(block);
		ASSERT_TRUE(not lines.empty());
		ASSERT_EQUAL_PRINT(version::renderShort("VM"), std::string(lines.front()));
	}

	void renderVerboseSkipsEmptyValuesTest() {
		// No licence has been chosen yet, so both licence rows must be missing rather than blank.
		const std::string block = version::renderVerbose("duckc");
		ASSERT_EQUAL(version::licenseName().empty(), block.find("\nlicense:") == std::string::npos);
		ASSERT_EQUAL(
			version::copyrightLine().empty(), block.find("\ncopyright:") == std::string::npos
		);

		// The same rule applies to a row the caller passes in.
		const std::array<version::ExtraField, 2> extra{
			version::ExtraField{ "filled", "yes" },
			version::ExtraField{ "blank", "" },
		};
		const std::string with_extra = version::renderVerbose("duckc", extra);
		ASSERT_TRUE(with_extra.find("\nfilled:") != std::string::npos);
		ASSERT_TRUE(with_extra.find("\nblank:") == std::string::npos);
	}

	void renderVerboseAppendsExtraRowsTest() {
		const std::array<version::ExtraField, 2> extra{
			version::ExtraField{ "LLVM", "19.1.7" },
			version::ExtraField{ "JIT", "enabled" },
		};
		const std::string block = version::renderVerbose("duckc", extra);
		const auto        lines = linesOf(block);

		ASSERT_TRUE(lines.size() >= 3);
		// Extra rows come last, in the order they were given.
		ASSERT_TRUE(lines[lines.size() - 2].starts_with("LLVM:"));
		ASSERT_TRUE(lines.back().starts_with("JIT:"));
		ASSERT_TRUE(lines.back().ends_with("enabled"));
		// A built-in row still comes first.
		ASSERT_TRUE(block.find("\nbuild-type:") != std::string::npos);
	}

	void renderVerboseAlignsValuesTest() {
		// "a-very-long-label" is longer than any built-in label, so it also checks that the
		// padding is computed from the extra rows and not only from the fixed ones.
		const std::array<version::ExtraField, 1> extra{
			version::ExtraField{ "a-very-long-label", "value" },
		};
		const std::string block = version::renderVerbose("duckc", extra);
		const auto        lines = linesOf(block);
		ASSERT_TRUE(lines.size() >= 2);

		const usize column = valueColumnOf(lines[1]);
		ASSERT_TRUE(column != std::string_view::npos);
		for (usize i = 1; i < lines.size(); ++i)
			ASSERT_EQUAL_PRINT(column, valueColumnOf(lines[i]));

		// The longest label keeps exactly one space between its colon and its value.
		const std::string_view longest = lines.back();
		ASSERT_EQUAL_PRINT(std::string("a-very-long-label: value"), std::string(longest));
	}
};

TESTER_COMMON_MAIN("/src/common/version/tests/");
