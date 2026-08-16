#include "packages.hpp"

#include "access.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/str/str_utils.hpp>

#include <hashing/add_to_hash.hpp>

#include <json/diagnostics.hpp>
#include <json/extract.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace compiler::frontend::packages {

	hashing::ComponentHash::HashType PackageInfo::computeHash(base::StrID package_id) {
		// @TODO: #2668 version and features are not included in the hash yet; they need
		// dedicated side inputs before generated code can depend on them.
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_id);
		return hasher.finalize();
	}

	PackageInfo::PackageInfo(
		compiler::frontend::ModuleID       root_module,
		base::StrID                        name,
		base::StrID                        version,
		std::vector<base::StrID>           features,
		std::vector<PackageDependencyInfo> dependencies
	):
		  m_root_module(root_module),
		  m_name(name),
		  m_version(version),
		  m_features(std::move(features)),
		  m_dependencies(std::move(dependencies)),
		  m_hash(computeHash(getModuleRef(root_module)->getPackage().illegalAccess().getID())) {}

	base::StrID PackageInfo::getPackageID() const {
		return getModuleRef(m_root_module)->getPackage().illegalAccess().getID();
	}

	base::StrID PackageInfo::getName() const { return m_name; }

	const hashing::ComponentHash::HashType& PackageInfo::getPackageHash() const { return m_hash; }

	compiler::frontend::ModuleAccessLocked PackageInfo::getRootModule() const {
		return compiler::frontend::ModuleAccessLocked(m_root_module);
	}

	base::StrID PackageInfo::getVersion() const { return m_version; }

	base::CRef<std::vector<base::StrID>> PackageInfo::getFeatures() const { return &m_features; }

	PackageDependenciesAccessLocked PackageInfo::getDependencies() const {
		auto                                       owner_id = getPackageID();
		std::vector<PackageDependencyAccessLocked> deps;
		deps.reserve(m_dependencies.size());
		for (const auto& dep: m_dependencies)
			deps.emplace_back(owner_id, dep.alias, dep.package_id);
		return { owner_id, std::move(deps) };
	}

	PackageDependencyAliasAccessLocked PackageInfo::getPackageDependencyByAlias(base::StrID alias
	) const {
		base::Optional<base::StrID> resolved;
		for (const auto& dep: m_dependencies)
			if (dep.alias == alias) {
				resolved = dep.package_id;
				break;
			}
		return { getPackageID(), alias, resolved };
	}

	base::Optional<RawDependencyInfo> RawDependencyInfo::fromJson(
		const nlohmann::json& json, const DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "dependency", report)) return {};

		js::checkForUnknownFields(json, { "id" }, { "alias" }, "dependency", report);

		bool had_error = false;

		auto id = js::getString(json, "id", "Dependency requires a package id!", report);
		if (!id) had_error = true;

		base::Optional<base::StrID> alias;
		if (auto alias_result = js::extractString(json, "alias"); alias_result.has_value()) {
			alias = *alias_result;
		} else if (alias_result.error() != js::JsonExtractError::MissingKey) {
			report("Dependency \"alias\" field must be a string.", std::string{}, true);
			had_error = true;
		}

		if (had_error) return {};

		return RawDependencyInfo{
			.package_id = *id,
			.alias      = alias,
		};
	}

	base::Optional<RawPackageInfo> RawPackageInfo::fromJson(
		const nlohmann::json& json, const DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "package", report)) return {};

		js::checkForUnknownFields(
			json,
			{ "id", "name", "path" },
			{ "version", "features", "dependencies" },
			"package",
			report
		);

		bool had_error = false;

		auto id = js::getString(json, "id", "Package requires an id!", report);
		if (!id) had_error = true;

		auto name = js::getString(json, "name", "Package requires a name!", report);
		if (!name) had_error = true;

		auto path = js::getString(json, "path", "Package requires a path!", report);
		if (!path) had_error = true;

		base::Optional<base::StrID> version_opt;
		if (auto version_result = js::extractString(json, "version"); version_result.has_value()) {
			version_opt = *version_result;
		} else if (version_result.error() != js::JsonExtractError::MissingKey) {
			report("Package \"version\" field must be a string.", std::string{}, true);
			had_error = true;
		}

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
		} else if (json.contains("dependencies")) {
			report(
				"Package dependencies must be an array",
				"The 'dependencies' field must be an array of objects when present.",
				true
			);
			had_error = true;
		}

		if (had_error) return {};

		return RawPackageInfo{
			.package_id   = *id,
			.package_name = *name,
			.version      = version_opt.has_value() ? *version_opt : base::StrID(),
			.package_path = fs::FilePath(path->str()),
			.features     = std::move(features),
			.dependencies = std::move(deps),
		};
	}

	base::Optional<PackageInfo> createPackageInfo(
		const RawPackageInfo&              package_info,
		const std::vector<RawPackageInfo>& all_packages,
		const DiagnosticReporter&          report
	) {
		auto root_module = compiler::frontend::createModuleTree(
			package_info.package_path, package_info.package_id
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
					".dm file to the package module directory."
				),
				true
			);
			return {};
		}

		std::vector<PackageDependencyInfo> package_dependencies;
		package_dependencies.reserve(package_info.dependencies.size());
		for (const auto& dependency: package_info.dependencies) {
			auto target_package = std::ranges::find_if(all_packages, [&](const auto& pkg) {
				return pkg.package_id == dependency.package_id;
			});
			if (target_package == all_packages.end()) CORE_PANIC("Unfiltered invalid dependency");
			package_dependencies.push_back(PackageDependencyInfo{
				.package_id = dependency.package_id,
				.alias      = dependency.alias.copyValueOr(target_package->package_name),
			});
		}

		return PackageInfo(
			root_module,
			package_info.package_name,
			package_info.version,
			package_info.features,
			std::move(package_dependencies)
		);
	}

	void filterUndeclaredDependencies(
		std::vector<RawPackageInfo>& packages_info, const DiagnosticReporter& report
	) {
		std::unordered_set<base::StrID> declared_ids;
		declared_ids.reserve(packages_info.size());
		for (const auto& package_info: packages_info) declared_ids.insert(package_info.package_id);

		for (auto& package_info: packages_info)
			std::erase_if(package_info.dependencies, [&](const RawDependencyInfo& dep) {
				if (declared_ids.contains(dep.package_id)) return false;
				report(
					base::strConcat(
						"Dropping dependency \"",
						dep.package_id.strView(),
						"\" of package \"",
						package_info.package_id.strView(),
						"\": no such package id is declared in the manifest."
					),
					std::string{},
					true
				);
				return true;
			});
	}

}  // namespace compiler::frontend::packages
