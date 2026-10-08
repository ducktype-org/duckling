// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "version.hpp"

// Regenerated on every build by the VersionCommitInfo target, so the commit date can never go
// stale. This is the only translation unit that may include it.
#include <version/commit_info.hpp>

#include <base/types/ints.hpp>

#include <algorithm>
#include <vector>

namespace {
	/** @brief Joins two views with a single space, skipping the space when one of them is empty. */
	std::string joinWithSpace(std::string_view left, std::string_view right) {
		if (left.empty()) return std::string(right);
		if (right.empty()) return std::string(left);
		return std::string(left) + " " + std::string(right);
	}
}  // namespace

namespace version {
	std::string_view semver() {
		static constexpr std::string_view VALUE{ DUCKLING_SEMVER };
		return VALUE;
	}

	std::string_view semverMajor() {
		static constexpr std::string_view VALUE{ DUCKLING_SEMVER_MAJOR };
		return VALUE;
	}

	std::string_view semverMinor() {
		static constexpr std::string_view VALUE{ DUCKLING_SEMVER_MINOR };
		return VALUE;
	}

	std::string_view semverPatch() {
		static constexpr std::string_view VALUE{ DUCKLING_SEMVER_PATCH };
		return VALUE;
	}

	std::string_view prerelease() {
		static constexpr std::string_view VALUE{ DUCKLING_PRERELEASE };
		return VALUE;
	}

	bool isPrerelease() { return not prerelease().empty(); }

	std::string_view commitHash() {
		static constexpr std::string_view VALUE{ DUCKLING_COMMIT_HASH };
		return VALUE;
	}

	std::string_view commitDate() {
		static constexpr std::string_view VALUE{ DUCKLING_COMMIT_DATE };
		return VALUE;
	}

	std::string_view buildType() {
		static constexpr std::string_view VALUE{ DUCKLING_BUILD_TYPE };
		return VALUE;
	}

	std::string_view hostSystem() {
		static constexpr std::string_view VALUE{ DUCKLING_HOST_SYSTEM };
		return VALUE;
	}

	std::string_view hostProcessor() {
		static constexpr std::string_view VALUE{ DUCKLING_HOST_PROCESSOR };
		return VALUE;
	}

	std::string_view compilerId() {
		static constexpr std::string_view VALUE{ DUCKLING_COMPILER_ID };
		return VALUE;
	}

	std::string_view compilerVersion() {
		static constexpr std::string_view VALUE{ DUCKLING_COMPILER_VERSION };
		return VALUE;
	}

	std::string_view licenseName() {
		static constexpr std::string_view VALUE{ DUCKLING_LICENSE_NAME };
		return VALUE;
	}

	std::string_view copyrightLine() {
		static constexpr std::string_view VALUE{ DUCKLING_COPYRIGHT_LINE };
		return VALUE;
	}

	std::string renderShort(std::string_view tool_name) {
		std::string result = joinWithSpace(tool_name, semver());

		const std::string commit = joinWithSpace(commitHash(), commitDate());
		if (not commit.empty()) {
			result += " (";
			result += commit;
			result += ')';
		}

		return result;
	}

	std::string renderVerbose(std::string_view tool_name, std::span<const ExtraField> extra) {
		const std::string host     = joinWithSpace(hostSystem(), hostProcessor());
		const std::string compiler = joinWithSpace(compilerId(), compilerVersion());

		std::vector<ExtraField> rows{
			{ "commit-hash", commitHash() },  { "commit-date", commitDate() },
			{ "build-type", buildType() },    { "host", host },
			{ "compiler", compiler },         { "license", licenseName() },
			{ "copyright", copyrightLine() },
		};
		rows.insert(rows.end(), extra.begin(), extra.end());

		std::erase_if(rows, [](const ExtraField& row) { return row.second.empty(); });

		usize label_width = 0;
		for (const auto& [label, VALUE]: rows) label_width = std::max(label_width, label.size());

		std::string result = renderShort(tool_name);
		for (const auto& [label, VALUE]: rows) {
			result += '\n';
			result += label;
			result += ':';
			// +1 so that even the longest label keeps one space before its VALUE.
			result.append(label_width - label.size() + 1, ' ');
			result += VALUE;
		}

		return result;
	}

}  // namespace version
