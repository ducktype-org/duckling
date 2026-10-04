/**
 * @file test_utils.hpp
 * @brief Small helpers shared by driver tests.
 */
#pragma once

#include <frontend/packages/packages.hpp>

#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <string_view>

namespace driver_test_utils {

	/** @brief Build a featureless, dependency-less RawPackageInfo for the given name and path. */
	inline compiler::frontend::packages::RawPackageInfo emptyRawPackageInfo(
		std::string_view name, std::string_view path
	) {
		return compiler::frontend::packages::RawPackageInfo{
			.package_id   = base::StrID(std::string(name)),
			.package_name = base::StrID(std::string(name)),
			.version      = base::StrID("not_supported"),
			.package_path = fs::FilePath(std::string(path)),
			.features     = {},
			.dependencies = {},
		};
	}

}  // namespace driver_test_utils
