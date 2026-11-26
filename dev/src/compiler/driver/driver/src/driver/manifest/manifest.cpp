// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "manifest.hpp"

#include <frontend/packages/packages.hpp>

#include <base/str/str_utils.hpp>

#include <json/diagnostics.hpp>
#include <nlohmann/json.hpp>

#include <unordered_set>
#include <utility>

namespace compiler::driver {

	namespace fp = compiler::frontend::packages;

	namespace {
		std::vector<fp::RawPackageInfo> parsePackages(
			const std::vector<nlohmann::json>&  packages_array,
			bool&                               had_fatal,
			const manifest::DiagnosticReporter& report
		) {
			std::vector<fp::RawPackageInfo> packages;
			packages.reserve(packages_array.size());
			for (const auto& pkg_json: packages_array) {
				auto pkg = fp::RawPackageInfo::fromJson(pkg_json, report);
				if (pkg)
					packages.push_back(std::move(*pkg));
				else
					had_fatal = true;
			}
			return packages;
		}

		/**
		 * @brief Checks dependency edges of one importer against the declared packages.
		 * @param importer Name of the importing package, used in messages.
		 */
		base::OkBad verifyDependencies(
			const std::vector<fp::RawDependencyInfo>&           dependencies,
			std::string_view                                    importer,
			const std::unordered_set<base::StrID>&              package_ids,
			const std::unordered_map<base::StrID, base::StrID>& id_to_name,
			const manifest::DiagnosticReporter&                 report
		) {
			base::OkBad                     result = base::OK;
			std::unordered_set<base::StrID> dep_ids;
			std::unordered_set<base::StrID> dep_aliases;
			for (const auto& dep: dependencies) {
				if (!package_ids.contains(dep.package_id)) {
					report(
						base::strConcat(
							"Unknown dependency package: ",
							dep.package_id.str(),
							" (in ",
							importer,
							")"
						),
						"Every dependency must have a corresponding entry in the packages list.",
						true
					);
					result = base::BAD;
				}
				if (!dep_ids.insert(dep.package_id).second) {
					report(
						base::strConcat(
							"Duplicate dependency: ", dep.package_id.str(), " (in ", importer, ")"
						),
						"A package cannot list the same dependency twice.",
						true
					);
					result = base::BAD;
				}
				auto        name_it = id_to_name.find(dep.package_id);
				base::StrID default_alias
					= name_it != id_to_name.end() ? name_it->second : dep.package_id;
				base::StrID effective_alias = dep.alias.copyValueOr(default_alias);
				if (!dep_aliases.insert(effective_alias).second) {
					report(
						base::strConcat(
							"Duplicate dependency alias: ",
							effective_alias.str(),
							" (in ",
							importer,
							")"
						),
						"A package cannot have two dependencies sharing the same alias. "
						"A dependency without an explicit alias uses its target package's name "
						"as the alias.",
						true
					);
					result = base::BAD;
				}
			}
			return result;
		}

		/**
		 * @brief Checks package id/name uniqueness and every package's dependency edges.
		 * @param session_dependencies Dependencies of the REPL session, checked like a package's;
		 * may be null.
		 */
		base::OkBad verifyPackages(
			const std::vector<fp::RawPackageInfo>&    packages,
			const std::vector<fp::RawDependencyInfo>* session_dependencies,
			const manifest::DiagnosticReporter&       report
		) {
			base::OkBad result = base::OK;

			std::unordered_set<base::StrID>              package_ids;
			std::unordered_set<base::StrID>              package_names;
			std::unordered_map<base::StrID, base::StrID> id_to_name;
			for (const auto& pkg: packages) {
				if (!package_ids.insert(pkg.package_id).second) {
					report(
						base::strConcat("Duplicate package id in manifest: ", pkg.package_id.str()),
						"Two or more packages share the same id in the packages list. "
						"Each package id must be unique.",
						true
					);
					result = base::BAD;
				}
				if (!package_names.insert(pkg.package_name).second) {
					report(
						base::strConcat(
							"Duplicate package name in manifest: ", pkg.package_name.str()
						),
						"Two or more packages share the same name in the packages list. "
						"Each package name must be unique.",
						true
					);
					result = base::BAD;
				}
				id_to_name[pkg.package_id] = pkg.package_name;
			}

			for (const auto& pkg: packages) {
				auto pkg_result = verifyDependencies(
					pkg.dependencies,
					base::strConcat("package ", pkg.package_name.str()),
					package_ids,
					id_to_name,
					report
				);
				if (pkg_result.isBad()) result = base::BAD;
			}

			if (session_dependencies != nullptr) {
				auto session_result = verifyDependencies(
					*session_dependencies, "session_dependencies", package_ids, id_to_name, report
				);
				if (session_result.isBad()) result = base::BAD;
			}

			return result;
		}
	}

	base::Optional<PackageCompilationManifest> PackageCompilationManifest::fromJson(
		const nlohmann::json& json, const manifest::DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "manifest", report)) return {};

		js::checkForUnknownFields(json, { "packages", "tasks" }, {}, "manifest", report);

		auto packages_array
			= js::getArray(json, "packages", "Manifest requires a packages array!", report);
		if (!packages_array) return {};

		bool had_fatal = false;
		auto packages  = parsePackages(*packages_array, had_fatal, report);

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

		if (verifyPackages(packages, nullptr, report).isBad()) result = base::BAD;
		return result;
	}

	base::Optional<ReplPackagesManifest> ReplPackagesManifest::fromJson(
		const nlohmann::json& json, const manifest::DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "manifest", report)) return {};

		js::checkForUnknownFields(
			json, { "packages", "session_dependencies" }, {}, "manifest", report
		);

		auto packages_array
			= js::getArray(json, "packages", "Manifest requires a packages array!", report);
		if (!packages_array) return {};

		bool had_fatal = false;
		auto packages  = parsePackages(*packages_array, had_fatal, report);

		auto session_array = js::getArray(
			json, "session_dependencies", "Manifest requires a session_dependencies array!", report
		);
		if (!session_array) return {};

		std::vector<fp::RawDependencyInfo> session_dependencies;
		session_dependencies.reserve(session_array->size());
		for (const auto& dep_json: *session_array) {
			auto dep = fp::RawDependencyInfo::fromJson(dep_json, report);
			if (dep)
				session_dependencies.push_back(*dep);
			else
				had_fatal = true;
		}

		if (had_fatal) return {};

		return ReplPackagesManifest{
			.packages             = std::move(packages),
			.session_dependencies = std::move(session_dependencies),
		};
	}

	base::OkBad ReplPackagesManifest::verify(const manifest::DiagnosticReporter& report) const {
		return verifyPackages(packages, &session_dependencies, report);
	}

}  // namespace compiler::driver
