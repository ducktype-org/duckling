#include "version.hpp"

// Regenerated on every build by the VersionCommitInfo target, so the commit date can never go
// stale. This is the only translation unit that may include it.
#include <version/commit_info.hpp>

#include <base/types/ints.hpp>

#include <algorithm>
#include <vector>

namespace {
	constexpr std::string_view SEMVER{ DUCKLING_SEMVER };
	constexpr std::string_view SEMVER_MAJOR{ DUCKLING_SEMVER_MAJOR };
	constexpr std::string_view SEMVER_MINOR{ DUCKLING_SEMVER_MINOR };
	constexpr std::string_view SEMVER_PATCH{ DUCKLING_SEMVER_PATCH };
	constexpr std::string_view PRERELEASE{ DUCKLING_PRERELEASE };
	constexpr std::string_view COMMIT_DATE{ DUCKLING_COMMIT_DATE };
	constexpr std::string_view BUILD_TYPE{ DUCKLING_BUILD_TYPE };
	constexpr std::string_view HOST_SYSTEM{ DUCKLING_HOST_SYSTEM };
	constexpr std::string_view HOST_PROCESSOR{ DUCKLING_HOST_PROCESSOR };
	constexpr std::string_view COMPILER_ID{ DUCKLING_COMPILER_ID };
	constexpr std::string_view COMPILER_VERSION{ DUCKLING_COMPILER_VERSION };
	constexpr std::string_view LICENSE_NAME{ DUCKLING_LICENSE_NAME };
	constexpr std::string_view COPYRIGHT_LINE{ DUCKLING_COPYRIGHT_LINE };

	/** @brief Joins two views with a single space, skipping the space when one of them is empty. */
	std::string joinWithSpace(std::string_view left, std::string_view right) {
		if (left.empty()) return std::string(right);
		if (right.empty()) return std::string(left);
		return std::string(left) + " " + std::string(right);
	}
}  // namespace

namespace version {

	std::string_view semver() { return SEMVER; }

	std::string_view semverMajor() { return SEMVER_MAJOR; }

	std::string_view semverMinor() { return SEMVER_MINOR; }

	std::string_view semverPatch() { return SEMVER_PATCH; }

	std::string_view prerelease() { return PRERELEASE; }

	bool isPrerelease() { return not PRERELEASE.empty(); }

	std::string_view commitDate() { return COMMIT_DATE; }

	bool isDirty() { return DUCKLING_IS_DIRTY; }

	std::string_view buildType() { return BUILD_TYPE; }

	std::string_view hostSystem() { return HOST_SYSTEM; }

	std::string_view hostProcessor() { return HOST_PROCESSOR; }

	std::string_view compilerId() { return COMPILER_ID; }

	std::string_view compilerVersion() { return COMPILER_VERSION; }

	std::string_view licenseName() { return LICENSE_NAME; }

	std::string_view copyrightLine() { return COPYRIGHT_LINE; }

	std::string renderShort(std::string_view tool_name) {
		std::string result = joinWithSpace(tool_name, semver());

		// Without git there is no date and no way to be dirty, so the whole group goes away.
		if (not commitDate().empty()) {
			result += " (";
			result += commitDate();
			if (isDirty()) result += "-dirty";
			result += ')';
		}

		return result;
	}

	std::string renderVerbose(std::string_view tool_name, std::span<const ExtraField> extra) {
		// host and compiler are composed from two getters each, so they need storage that
		// outlives the views collected below.
		const std::string host     = joinWithSpace(hostSystem(), hostProcessor());
		const std::string compiler = joinWithSpace(compilerId(), compilerVersion());

		std::vector<ExtraField> rows{
			{ "commit-date", commitDate() },
			{ "build-type", buildType() },
			{ "host", host },
			{ "compiler", compiler },
			{ "license", licenseName() },
			{ "copyright", copyrightLine() },
		};
		rows.insert(rows.end(), extra.begin(), extra.end());

		// A fact we do not have (no licence chosen yet, no git worktree) is left out entirely
		// rather than printed as an empty line.
		std::erase_if(rows, [](const ExtraField& row) { return row.second.empty(); });

		usize label_width = 0;
		for (const auto& [label, value]: rows) label_width = std::max(label_width, label.size());

		std::string result = renderShort(tool_name);
		for (const auto& [label, value]: rows) {
			result += '\n';
			result += label;
			result += ':';
			// +1 so that even the longest label keeps one space before its value.
			result.append(label_width - label.size() + 1, ' ');
			result += value;
		}

		return result;
	}

}  // namespace version
