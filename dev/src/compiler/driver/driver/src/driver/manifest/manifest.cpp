#include "manifest.hpp"

#include "utils.hpp"

#include <base/pointers/box.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <json/json.hpp>

namespace compiler::driver {

	namespace ju = compiler::driver::json;

	base::Optional<PackageCompilationManifest> PackageCompilationManifest::fromJson(
		const nlohmann::json& json
	) {
		if (!json.is_object()) {
			if (global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(
					makeBox<dia_int::PlaceholderHeaderError>(
						"Manifest must be a JSON object",
						"The top-level manifest value must be a JSON object."
					)
				);
			}
			return {};
		}

		ju::verifyNumberOfFields(json, { "packages", "tasks" }, {}, "manifest");

		auto packages_array = ju::getArray(json, "packages", "Manifest requires a packages array!");
		if (!packages_array) return {};

		std::vector<RawPackageInfo> packages;
		packages.reserve(packages_array->size());
		for (const auto& pkg_json : *packages_array) {
			auto pkg = RawPackageInfo::fromJson(pkg_json);
			if (!pkg) return {};
			packages.push_back(std::move(*pkg));
		}

		auto tasks_array = ju::getArray(json, "tasks", "Manifest requires a tasks array!");
		if (!tasks_array) return {};

		std::vector<Task> tasks;
		tasks.reserve(tasks_array->size());
		for (const auto& task_json : *tasks_array) {
			auto task = Task::fromJson(task_json);
			if (!task) return {};
			tasks.push_back(std::move(*task));
		}

		return PackageCompilationManifest{
			.packages = std::move(packages),
			.tasks    = std::move(tasks),
		};
	}

}  // namespace compiler::driver
