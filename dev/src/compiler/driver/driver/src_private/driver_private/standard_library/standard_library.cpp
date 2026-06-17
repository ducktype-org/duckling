#include "standard_library.hpp"

#include <driver/task/task.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <algorithm>

namespace compiler::driver {

	/**
	 * @brief Configuration for the standard library packages.
	 * It acts as a single source of truth for the standard library packages,
	 * but should not be used directly outside the standard library handling code.
	 */
	const std::array<STDPackageConfig, 2> STD_PACKAGES_CONFIG = {
		{
			{
				.name         = "std",
				.subpath      = "std",
				.dependencies = { "core" },
			},
			{
				.name         = "core",
				.subpath      = "core",
				.dependencies = {},
			},
		},
	};

	namespace {
		fs::FilePath resolveDefaultStdPath() {
#ifdef STD_FIXED_PATH
			return { STD_FIXED_PATH };
#else
			return getExecutablePath().parentPath().parentPath().join(fs::FilePath("lib/core/std"));
#endif
		}
	}

	base::Optional<fs::FilePath> resolveStdPath(
		const options_types::StdLibOptions& standard_library_options
	) {
		base::Optional<fs::FilePath> path;
		variant_match(standard_library_options.std_lib_type) {
			variant_case(options_types::StdLibOptions::NoStd, _) { return {}; }
			variant_case(options_types::StdLibOptions::CustomStd, custom_std) {
				path = custom_std.std_path;
			}
			variant_case(options_types::StdLibOptions::DefaultStd, _) {
				path = resolveDefaultStdPath();
			}
			variant_default { CORE_UNREACHABLE(); }
		}
		return path;
	}

	namespace {

		bool hasDependencyOrAlias(
			const std::vector<compiler::frontend::packages::RawDependencyInfo>& dependencies,
			base::StrID                                                         target_package_name,
			base::StrID                                                         target_package_id
		) {
			return std::ranges::any_of(dependencies, [&](const auto& dep) {
				return dep.package_id == target_package_id
				    or (dep.alias.has_value() && *dep.alias == target_package_name);
			});
		}

		bool hasPackageNameOrID(
			const std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
			base::StrID                                                      name,
			base::StrID                                                      id
		) {
			return std::ranges::any_of(
				packages_info,
				[&](const compiler::frontend::packages::RawPackageInfo& package) {
					return package.package_id == id or package.package_name == name;
				}
			);
		}

		base::OkBad addDependenciesOnStandardLibraryForPackage(
			compiler::frontend::packages::RawPackageInfo& package_info,
			frontend::packages::DiagnosticReporter&       report
		) {
			// If the package is one of the STD_PACKAGES_CONFIG, we do nothing
			if (std::ranges::any_of(STD_PACKAGES_CONFIG, [&](const auto& config) {
					return base::StrID(config.name) == package_info.package_id;
				})) {
				return base::OK;
			}

			// Otherwise, we add dependencies on all standard library packages
			for (auto& config: STD_PACKAGES_CONFIG) {
				auto std_name = base::StrID(config.name);
				auto std_id   = base::StrID(config.name);
				if (hasDependencyOrAlias(package_info.dependencies, std_name, std_id)) {
					report(
						"Standard library package name is already used as an external dependency.",
						base::strConcat(
							"Package '", package_info.package_name, "' depends on '", std_name
						),
						true
					);
					return base::BAD;
				}
				package_info.dependencies.push_back(compiler::frontend::packages::RawDependencyInfo{
					.package_id = std_id,
					.alias      = {},
				});
			}
			return base::OK;
		}

		/**
		 * @brief To the `packages_info` vector, to each `RawPackageInfo` in it
		 * adds the dependency on all standard library packages. This way this dependency
		 * doesn't have to be provided by the user in the manifest and is added by the compiler.
		 */
		base::OkBad addDependenciesOnStandardLibrary(
			std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
			frontend::packages::DiagnosticReporter&                    report

		) {
			for (auto& package_info: packages_info)
				if (addDependenciesOnStandardLibraryForPackage(package_info, report).isBad())
					return base::BAD;
			return base::OK;
		}

		/**
		 * @brief Creates packages for the standard library and adds them to the provided vector.
		 * @param packages_info[out] The std lib packages will be added here as dependencies.
		 * @param std_path Path to the standard library.
		 * @param report Diagnostic reporter to report any issues with the standard library packages
		 * (like a missing package).
		 */
		base::OkBad appendStandardLibraryPackages(
			std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
			const fs::FilePath&                                        std_path,
			frontend::packages::DiagnosticReporter&                    report
		) {
			using compiler::frontend::packages::RawDependencyInfo;
			using compiler::frontend::packages::RawPackageInfo;

			for (auto& config: STD_PACKAGES_CONFIG) {
				if (hasPackageNameOrID(
						packages_info, base::StrID(config.name), base::StrID(config.name)
					)) {
					report(
						base::strConcat("Package with name `", config.name, "` already exist."),
						"",
						true
					);
					return base::BAD;
				}
			}

			for (auto& config: STD_PACKAGES_CONFIG) {
				auto package_path = std_path.join(fs::FilePath(config.subpath));
				if (not package_path.exists()) {
					report(
						"Standard library package path does not exist.",
						base::strConcat(
							"Expected standard library package at: ", package_path.string()
						),
						true
					);
					return base::BAD;
				}
				RawPackageInfo raw_package_info{
					.package_id   = base::StrID(config.name),
					.package_name = base::StrID(config.name),
					.version      = base::StrID("not_supported"),
					.package_path = package_path,
					.features     = {},
					.dependencies = {},
				};

				for (const auto& dep_name: config.dependencies) {
					raw_package_info.dependencies.push_back(RawDependencyInfo{
						.package_id = base::StrID(dep_name),
						.alias      = {},
					});
				}
				packages_info.push_back(std::move(raw_package_info));
			}
			return base::OK;
		}
	}

	base::OkBad addStandardLibraryPackages(
		std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
		const fs::FilePath&                                        std_path,
		frontend::packages::DiagnosticReporter&                    report
	) {
		if (addDependenciesOnStandardLibrary(packages_info, report).isBad()) return base::BAD;
		return appendStandardLibraryPackages(packages_info, std_path, report);
	}

	std::vector<PackageCompilationTask> getStandardLibraryCompilationTasks() {
		std::vector<PackageCompilationTask> tasks;
		for (const auto& config: STD_PACKAGES_CONFIG) {
			auto pkg = std::find_if(
				global_state::getPackages().begin(),
				global_state::getPackages().end(),
				[&](const auto& pkg_info) {
					return pkg_info.getPackageID() == base::StrID(config.name);
				}
			);
			CORE_ASSERT(
				pkg != global_state::getPackages().end(),
				"Standard library package not found in global state"
			);

			tasks.push_back(
				PackageCompilationTask{
					.root_module  = pkg->getRootModule().illegalAccess().getID(),
					.build_target = BuildTargetLLVMStaticLibrary{ .output_file_name
			                                                      = base::StrID(config.name + ".a"),
			                                                      .archiving_options = {}, },
				}
			);
			tasks.push_back(
				PackageCompilationTask{
					.root_module = pkg->getRootModule().illegalAccess().getID(),
					.build_target
					= BuildTargetDVM{ .output_file_name = base::StrID(config.name + ".a"), },
				}
			);
		}
		return tasks;
	}

	base::Optional<std::string> getStdLibLinkingArgs(
		const options_types::StdLibOptions& linking_options
	) {
		if_opt_some(resolveStdPath(linking_options), _) {
			std::string result;
			for (const auto& art: getStdLibArtifacts())
				result += " " + art.file.getFilePath().string();
			return result;
		}
		return {};
	}

	std::vector<artifacts::FileArtifact> getStdLibArtifacts() {
		std::vector<artifacts::FileArtifact> artifacts;
		for (const auto& config: STD_PACKAGES_CONFIG) {
			auto artifact = global_state::getRootCollection()->fileArtifactAtOrNew(
				base::StrID(base::strConcat(config.name, ".a"))
			);
			artifacts.push_back(std::move(artifact));
		}
		return artifacts;
	}
}
