#include "manifest.hpp"

#include "utils.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>

#include <base/pointers/box.hpp>

#include <json/json.hpp>

#include <unordered_set>

namespace compiler::driver {

	namespace ju = compiler::driver::json;

	base::Optional<PackageCompilationManifest> PackageCompilationManifest::fromJson(
		const nlohmann::json& json
	) {
		if (!ju::checkIsObject(json, "manifest")) return {};

		ju::checkForUknownFields(json, { "packages", "tasks" }, {}, "manifest");

		auto packages_array = ju::getArray(json, "packages", "Manifest requires a packages array!");
		if (!packages_array) return {};

		std::vector<RawPackageInfo> packages;
		packages.reserve(packages_array->size());
		for (const auto& pkg_json: *packages_array) {
			auto pkg = RawPackageInfo::fromJson(pkg_json);
			if (!pkg) return {};
			packages.push_back(std::move(*pkg));
		}

		auto tasks_array = ju::getArray(json, "tasks", "Manifest requires a tasks array!");
		if (!tasks_array) return {};

		std::vector<RawTask> tasks;
		tasks.reserve(tasks_array->size());
		for (const auto& task_json: *tasks_array) {
			auto task = RawTask::fromJson(task_json);
			if (!task) return {};
			tasks.push_back(std::move(*task));
		}

		return PackageCompilationManifest{
			.packages = std::move(packages),
			.tasks    = std::move(tasks),
		};
	}

	namespace {
		void logManifestError(const std::string& header, const std::string& description) {
			if (!global_state::hasGlobalLogger()) return;
			global_state::getGlobalLogger()->log(
				makeBox<dia_int::PlaceholderError>(header, description)
			);
		}
	}  // namespace

	base::OkBad PackageCompilationManifest::verify() const {
		base::OkBad result = base::OK;

		if (packages.empty()) {
			logManifestError(
				"Empty packages list in manifest", "The manifest must contain at least one package."
			);
			result = base::BAD;
		}

		std::unordered_set<base::StrID> package_names;
		for (const auto& pkg: packages) {
			if (!package_names.insert(pkg.package_name).second) {
				logManifestError(
					base::strConcat("Duplicate package name in manifest: ", pkg.package_name.str()),
					"Two or more packages share the same name in the packages list. "
					"Each package name must be unique."
				);
				result = base::BAD;
			}
		}

		for (const auto& pkg: packages) {
			std::unordered_set<base::StrID> dep_package_names;
			std::unordered_set<base::StrID> dep_aliases;
			for (const auto& dep: pkg.dependencies) {
				if (!package_names.contains(dep.package_name)) {
					logManifestError(
						base::strConcat(
							"Unknown dependency package: ",
							dep.package_name.str(),
							" (in package ",
							pkg.package_name.str(),
							")"
						),
						"Every dependency must have a corresponding entry in the packages list."
					);
					result = base::BAD;
				}
				if (!dep_package_names.insert(dep.package_name).second) {
					logManifestError(
						base::strConcat(
							"Duplicate dependency: ",
							dep.package_name.str(),
							" (in package ",
							pkg.package_name.str(),
							")"
						),
						"A package cannot list the same dependency twice."
					);
					result = base::BAD;
				}
				const base::StrID effective_alias
					= dep.alias.isBad() ? dep.package_name : dep.alias;
				if (!dep_aliases.insert(effective_alias).second) {
					logManifestError(
						base::strConcat(
							"Duplicate dependency alias: ",
							effective_alias.str(),
							" (in package ",
							pkg.package_name.str(),
							")"
						),
						"A package cannot have two dependencies sharing the same alias. "
						"A dependency without an explicit alias uses its package name as the alias."
					);
					result = base::BAD;
				}
			}
		}

		return result;
	}

}  // namespace compiler::driver
