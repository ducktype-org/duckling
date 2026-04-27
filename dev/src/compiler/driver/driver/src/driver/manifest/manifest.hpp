#pragma once
#include "driver/packages/packages.hpp"
#include "driver/task/task.hpp"

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

#include <vector>

namespace compiler::driver {

	struct PackageCompilationManifest final {
		std::vector<RawPackageInfo> packages;
		std::vector<Task>            tasks;

		static base::Optional<PackageCompilationManifest> fromJson(const nlohmann::json& json);
	};
}
