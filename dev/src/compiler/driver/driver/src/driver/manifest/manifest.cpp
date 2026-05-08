#include "manifest.hpp"

#include <frontend/packages/packages.hpp>

#include <base/str/str_utils.hpp>

#include <json/diagnostics.hpp>
#include <nlohmann/json.hpp>

#include <unordered_set>
#include <utility>

namespace compiler::driver {

	namespace fp = compiler::frontend::packages;

	base::Optional<PackageCompilationManifest> PackageCompilationManifest::fromJson(
		const nlohmann::json& json, const manifest::DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "manifest", report)) return {};

		js::checkForUnknownFields(json, { "packages", "tasks" }, {}, "manifest", report);

		auto packages_array
			= js::getArray(json, "packages", "Manifest requires a packages array!", report);
		if (!packages_array) return {};

		bool                            had_fatal = false;
		std::vector<fp::RawPackageInfo> packages;
		packages.reserve(packages_array->size());
		for (const auto& pkg_json: *packages_array) {
			auto pkg = fp::RawPackageInfo::fromJson(pkg_json, report);
			if (pkg)
				packages.push_back(std::move(*pkg));
			else
				had_fatal = true;
		}

		auto tasks_array = js::getArray(json, "tasks", "Manifest requires a tasks array!", report);
		if (!tasks_array) return {};

		std::vector<RawTask> tasks;
		tasks.reserve(tasks_array->size());
		for (const auto& task_json: *tasks_array) {
			auto task = RawTask::fromJson(task_json, report);
			if (task)
				tasks.push_back(std::move(*task));
			else
				had_fatal = true;
		}

		if (had_fatal) return {};

		return PackageCompilationManifest{
			.packages = std::move(packages),
			.tasks    = std::move(tasks),
		};
	}

	base::OkBad PackageCompilationManifest::verify(const manifest::DiagnosticReporter& report
	) const {
		base::OkBad result = base::OK;

		if (packages.empty()) {
			report(
				"Empty packages list in manifest",
				"The manifest must contain at least one package.",
				true
			);
			result = base::BAD;
		}

		std::unordered_set<base::StrID> package_names;
		for (const auto& pkg: packages) {
			if (!package_names.insert(pkg.package_name).second) {
				report(
					base::strConcat("Duplicate package name in manifest: ", pkg.package_name.str()),
					"Two or more packages share the same name in the packages list. "
					"Each package name must be unique.",
					true
				);
				result = base::BAD;
			}
		}

		for (const auto& pkg: packages) {
			std::unordered_set<base::StrID> dep_package_names;
			std::unordered_set<base::StrID> dep_aliases;
			for (const auto& dep: pkg.dependencies) {
				if (!package_names.contains(dep.package_name)) {
					report(
						base::strConcat(
							"Unknown dependency package: ",
							dep.package_name.str(),
							" (in package ",
							pkg.package_name.str(),
							")"
						),
						"Every dependency must have a corresponding entry in the packages list.",
						true
					);
					result = base::BAD;
				}
				if (!dep_package_names.insert(dep.package_name).second) {
					report(
						base::strConcat(
							"Duplicate dependency: ",
							dep.package_name.str(),
							" (in package ",
							pkg.package_name.str(),
							")"
						),
						"A package cannot list the same dependency twice.",
						true
					);
					result = base::BAD;
				}
				const base::StrID effective_alias
					= dep.alias.isBad() ? dep.package_name : dep.alias;
				if (!dep_aliases.insert(effective_alias).second) {
					report(
						base::strConcat(
							"Duplicate dependency alias: ",
							effective_alias.str(),
							" (in package ",
							pkg.package_name.str(),
							")"
						),
						"A package cannot have two dependencies sharing the same alias. "
						"A dependency without an explicit alias uses its package name as the "
						"alias.",
						true
					);
					result = base::BAD;
				}
			}
		}

		return result;
	}

}  // namespace compiler::driver
