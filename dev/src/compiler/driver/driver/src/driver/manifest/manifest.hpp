#pragma once
#include "driver/packages/packages.hpp"
#include "driver/task/task.hpp"

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

#include <vector>

namespace compiler::driver {

	/**
	 * @brief Represents the manifest for package compilation in the Duckling compiler.
	 * The manifest contains information about the packages used in compilation processes and the tasks to be performed (like compile a single package).
	 */
	struct PackageCompilationManifest final {
		/**
		 * @brief A list of packages used in the compilation process.
		 * This includes the packages that are being compiled as well as their dependencies.
		 * Every dependency should have its own entry in this list, even if it's not a direct compilation target.
		 */
		std::vector<RawPackageInfo> packages;
		
		/**
		 * @brief A list of tasks to be performed during compilation.
		 */
		std::vector<RawTask>         tasks;

		static base::Optional<PackageCompilationManifest> fromJson(const nlohmann::json& json);
	};
}
