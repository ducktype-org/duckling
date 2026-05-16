#pragma once

#include <filesystem/file_path.hpp>

#include <array>

namespace compiler::driver {
	struct StdPackageConfig {
		std::string_view                        name;
		std::string_view                        subpath;
		std::initializer_list<std::string_view> dependencies;
	};

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

	fs::FilePath getExecutablePath();
}
