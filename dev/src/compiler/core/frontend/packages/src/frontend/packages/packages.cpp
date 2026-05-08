#include "packages.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/str/str_utils.hpp>

#include <json/diagnostics.hpp>
#include <json/extract.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace compiler::frontend::packages {

	base::Optional<RawDependencyInfo> RawDependencyInfo::fromJson(
		const nlohmann::json& json, const DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "dependency", report)) return {};

		js::checkForUnknownFields(json, { "name" }, { "alias" }, "dependency", report);

		bool had_error = false;

		auto name = js::getString(json, "name", "Dependency requires a package name!", report);
		if (!name) had_error = true;

		base::Optional<base::StrID> alias;
		if (auto alias_result = js::extractString(json, "alias"); alias_result.has_value())
			alias = *alias_result;

		if (had_error) return {};

		return RawDependencyInfo{
			.package_name = *name,
			.alias        = alias.has_value() ? *alias : base::StrID(),
		};
	}

	base::Optional<RawPackageInfo> RawPackageInfo::fromJson(
		const nlohmann::json& json, const DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "package", report)) return {};

		js::checkForUnknownFields(
			json, { "name", "path" }, { "version", "features", "dependencies" }, "package", report
		);

		bool had_error = false;

		auto name = js::getString(json, "name", "Package requires a name!", report);
		if (!name) had_error = true;

		auto path = js::getString(json, "path", "Package requires a path!", report);
		if (!path) had_error = true;

		base::Optional<base::StrID> version_opt;
		if (auto version_result = js::extractString(json, "version"); version_result.has_value())
			version_opt = *version_result;

		std::vector<base::StrID> features;
		if (auto features_result = js::extractArray(json, "features"); features_result.has_value()) {
			auto& features_array = *features_result;
			features.reserve(features_array.size());
			for (const auto& feat: features_array) {
				auto feat_str = js::getStringFromArray(
					feat,
					base::strConcat("package \"", name ? name->str() : std::string{}, "\" features"),
					report
				);
				if (feat_str)
					features.push_back(*feat_str);
				else
					had_error = true;
			}
		}

		std::vector<RawDependencyInfo> deps;
		if (auto deps_result = js::extractArray(json, "dependencies"); deps_result.has_value()) {
			auto& deps_array = *deps_result;
			deps.reserve(deps_array.size());
			for (const auto& dep_json: deps_array) {
				auto dep = RawDependencyInfo::fromJson(dep_json, report);
				if (dep)
					deps.push_back(*dep);
				else
					had_error = true;
			}
		}

		if (had_error) return {};

		return RawPackageInfo{
			.package_name = *name,
			.version      = version_opt.has_value() ? *version_opt : base::StrID(),
			.package_path = fs::FilePath(path->str()),
			.features     = std::move(features),
			.dependencies = std::move(deps),
		};
	}

	base::Optional<PackageInfo> createPackageInfo(
		const RawPackageInfo& package_info, const DiagnosticReporter& report
	) {
		auto root_module = compiler::frontend::createModuleTree(
			package_info.package_path, package_info.package_name
		);

		if (!getModuleRef(root_module)->hasMainSourceFile()) {
			auto module_name = getModuleRef(root_module)->getName();
			report(
				"Package does not have a main source file.",
				base::strConcat(
					"The main source file is required for package ",
					module_name,
					". Please add a ",
					module_name,
					".dmf file to the package module directory."
				),
				true
			);
			return {};
		}

		std::vector<PackageDependencyInfo> package_dependencies;
		package_dependencies.reserve(package_info.dependencies.size());
		for (const auto& dependency: package_info.dependencies) {
			package_dependencies.push_back(PackageDependencyInfo{
				.package_id = dependency.package_name,
				.alias      = dependency.alias.isBad() ? dependency.package_name : dependency.alias,
			});
		}

		return PackageInfo{
			.root_module      = root_module,
			.version          = package_info.version,
			.package_features = package_info.features,
			.dependencies     = std::move(package_dependencies),
		};
	}

}  // namespace compiler::frontend::packages
