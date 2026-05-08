#include "packages.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <driver/manifest/utils.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <json/json.hpp>

#include <utility>
#include <vector>

namespace compiler::driver {

	namespace ju = compiler::driver::json;

	base::Optional<RawDependencyInfo> RawDependencyInfo::fromJson(const nlohmann::json& json) {
		if (!ju::checkIsObject(json, "dependency")) return {};

		ju::checkForUknownFields(
			json,
			{
				"name",
			},
			{
				"alias",
			},
			"dependency"
		);

		auto name = ju::getString(json, "name", "Dependency requires a package name!");
		if (!name) return {};

		auto alias = ju::getStringNoError(json, "alias");

		return RawDependencyInfo{
			.package_name = *name,
			.alias        = alias.has_value() ? *alias : base::StrID(),
		};
	}

	base::Optional<RawPackageInfo> RawPackageInfo::fromJson(const nlohmann::json& json) {
		if (!ju::checkIsObject(json, "package")) return {};

		ju::checkForUknownFields(
			json,
			{
				"name",
				"path",
			},
			{
				"version",
				"features",
				"dependencies",
			},
			"package"
		);

		auto name = ju::getString(json, "name", "Package requires a name!");
		if (!name) return {};

		auto path = ju::getString(json, "path", "Package requires a path!");
		if (!path) return {};

		auto version = ju::getStringNoError(json, "version");
		if (!version) version = base::StrID();

		auto features_array = ju::getArrayNoError(json, "features");
		if (!features_array) features_array = std::vector<nlohmann::json>();

		std::vector<base::StrID> features;
		features.reserve(features_array->size());
		for (const auto& feat: *features_array) {
			auto feat_str = ju::getStringFromArray(
				feat, base::strConcat("package \"", name->str(), "\" features")
			);
			if (!feat_str) return {};
			features.push_back(*feat_str);
		}

		auto deps_array = ju::getArrayNoError(json, "dependencies");
		if (!deps_array) deps_array = std::vector<nlohmann::json>();

		std::vector<RawDependencyInfo> deps;
		deps.reserve(deps_array->size());
		for (const auto& dep_json: *deps_array) {
			auto dep = RawDependencyInfo::fromJson(dep_json);
			if (!dep) continue;  // Log error inside fromJson and skip invalid dependency
			deps.push_back(*dep);
		}

		return RawPackageInfo{
			.package_name = *name,
			.version      = *version,
			.package_path = fs::FilePath(path->str()),
			.features     = std::move(features),
			.dependencies = std::move(deps),
		};
	}

	base::Optional<global_state::PackageInfo> createGlobalPackageInfo(
		const RawPackageInfo& package_info
	) {
		// First, create the module tree for the package for the give path
		auto root_module = compiler::frontend::createModuleTree(
			package_info.package_path, package_info.package_name
		);

		// If module tree does not have a main source file, it is not a valid package. We require
		// main source file as an entry point for the package.
		if (!getModuleRef(root_module)->hasMainSourceFile()) {
			auto module_name = getModuleRef(root_module)->getName();
			global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
				"Package does not have a main source file.",
				base::strConcat(
					"The main source file is required for package ",
					module_name,
					". Please add a ",
					module_name,
					".dmf file to the package module directory."
				)
			));
			return {};
		}

		std::vector<global_state::PackageDependencyInfo> package_dependencies;

		for (const auto& dependency: package_info.dependencies) {
			auto dependency_package_info = global_state::PackageDependencyInfo{
				.package_id = dependency.package_name,
				.alias = (dependency.alias.isBad()) ? dependency.package_name : dependency.alias,
			};
			package_dependencies.push_back(dependency_package_info);
		}

		auto global_package_info = global_state::PackageInfo{
			.root_module      = root_module,
			.version          = package_info.version,
			.package_features = package_info.features,
			.dependencies     = std::move(package_dependencies),
		};
		return global_package_info;
	}

}  // namespace compiler::driver
