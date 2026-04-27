#include "packages.hpp"

#include <driver/manifest/utils.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <json/json.hpp>

namespace compiler::driver {

	namespace ju = compiler::driver::json;

	base::Optional<RawDependencyInfo> RawDependencyInfo::fromJson(const nlohmann::json& json) {
		ju::verifyNumberOfFields(json, { "name", "alias" }, {}, "dependency");

		auto name = ju::getString(json, "name", "Dependency requires a package name!");
		if (!name) return {};

		auto alias = ju::getString(json, "alias", "Dependency requires an alias!");
		if (!alias) return {};

		return RawDependencyInfo{
			.package_name = *name,
			.alias        = *alias,
		};
	}

	base::Optional<RawPackageInfo> RawPackageInfo::fromJson(const nlohmann::json& json) {
		ju::verifyNumberOfFields(
			json,
			{ "name", "version", "path", "features", "dependencies" },
			{ "main", "compilation_strategy" },
			"package"
		);

		auto name = ju::getString(json, "name", "Package requires a name!");
		if (!name) return {};

		auto version = ju::getString(json, "version", "Package requires a version!");
		if (!version) return {};

		auto path = ju::getString(json, "path", "Package requires a path!");
		if (!path) return {};

		auto features_array = ju::getArray(json, "features", "Package requires a features array!");
		if (!features_array) return {};

		std::vector<base::StrID> features;
		features.reserve(features_array->size());
		for (const auto& feat : *features_array) {
			if (!feat.is_string()) {
				if (global_state::hasGlobalLogger()) {
					global_state::getGlobalLogger()->log(
						makeBox<dia_int::PlaceholderHeaderError>(
							"Package features must be strings",
							base::strConcat("Package \"", name->str(), "\" has a non-string feature.")
						)
					);
				}
				return {};
			}
			features.emplace_back(feat.get<std::string>());
		}

		auto deps_array = ju::getArray(json, "dependencies", "Package requires a dependencies array!");
		if (!deps_array) return {};

		std::vector<RawDependencyInfo> deps;
		deps.reserve(deps_array->size());
		for (const auto& dep_json : *deps_array) {
			auto dep = RawDependencyInfo::fromJson(dep_json);
			if (!dep) return {};
			deps.push_back(std::move(*dep));
		}

		return RawPackageInfo{
			.package_name = *name,
			.version      = *version,
			.package_path = fs::FilePath(path->str()),
			.features     = std::move(features),
			.dependencies = std::move(deps),
		};
	}

}  // namespace compiler::driver
