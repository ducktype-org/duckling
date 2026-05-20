/**
 * @file standard_library.hpp
 * @brief The private header for the standard library handling in the Duckling compiler.
 */
#pragma once

#include <filesystem/file_path.hpp>

#include <array>

namespace compiler::driver {
	/**
	 * @brief Configuration for the standard library packages.
	 */
	struct StdPackageConfig {
		std::string_view                        name;
		std::string_view                        subpath;
		std::initializer_list<std::string_view> dependencies;
	};

	/**
	 * @brief Configuration for the standard library packages.
	 * It acts as a single source of truth for the standard library packages,
	 * but should not be used directly outside the standard library handling code.
	 */
	inline constexpr const std::array<StdPackageConfig, 2> STD_PACKAGES_CONFIG
		= { { {
				  .name         = "core",
				  .subpath      = "core",
				  .dependencies = {},
			  },
		      {
				  .name         = "std",
				  .subpath      = "std",
				  .dependencies = { "core" },
			  } } };

	/**
	 * @brief Helper that returns the path to the executable that is currently running.
	 * Works on all platforms.
	 */
	fs::FilePath getExecutablePath();
}
