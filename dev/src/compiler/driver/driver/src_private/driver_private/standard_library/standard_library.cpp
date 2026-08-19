#include "standard_library.hpp"

#include <driver/task/task.hpp>
#include <frontend/packages/standard_packages.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <artifacts/artifacts.hpp>
#include <logger/logger.hpp>
#include <os_utils/executable_path.hpp>

#include <algorithm>

namespace compiler::driver {

	namespace {
		fs::FilePath resolveDefaultStdPath() {
#ifdef STD_FIXED_PATH
			#error "aaa"
			return { STD_FIXED_PATH };
#else
			#warning "STD_FIXED_PATH is not defined"
			// #error "STD_FIXED_PATH is not defined. Please define it in the build system."
			return os_utils::getExecutablePath().parentPath().join(fs::FilePath("duck_lib/"));
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
			// If the package is one of the standard library packages, we do nothing
			if (frontend::packages::isStandardLibraryPackage(package_info.package_id))
				return base::OK;

			// Otherwise, we add dependencies on all standard library packages
			for (const auto& std_id: frontend::packages::standardLibraryPackageIds()) {
				if (hasDependencyOrAlias(package_info.dependencies, std_id, std_id)) {
					report(
						"Standard library package name is already used as an external dependency.",
						base::strConcat(
							"Package '", package_info.package_name, "' depends on '", std_id
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

			for (const auto& std_id: frontend::packages::standardLibraryPackageIds()) {
				if (hasPackageNameOrID(packages_info, std_id, std_id)) {
					report(
						base::strConcat("Package with name `", std_id, "` already exist."), "", true
					);
					return base::BAD;
				}
			}

			for (const auto& std_package: frontend::packages::standardLibraryPackages()) {
				// A standard library package lives in a directory named after its id.
				auto package_path
					= std_path.join(fs::FilePath(std::string(std_package.id.strView())));
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
					.package_id   = std_package.id,
					.package_name = std_package.id,
					.version      = base::StrID("not_supported"),
					.package_path = package_path,
					.features     = {},
					.dependencies = {},
				};

				for (const auto& dependency_id: std_package.dependencies) {
					raw_package_info.dependencies.push_back(RawDependencyInfo{
						.package_id = dependency_id,
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

	std::vector<PackageCompilationTask> getRequiredStdLibCompilationTasks() {
		base::Ref<artifacts::ArtifactCollection> art_collection = global_state::getRootCollection();
		if_opt_some(global_state::getStdArtifactsCollection(), stdlib_art_collection) {
			// If we use custom artifacts location and the required artifacts are present,
			// we don't require any tasks.
			if (allStdlibArtifactsPresent()) {
				CORE_USER_LOG(
					"Using compiled standard library binaries from the custom directory.\n"
				);
				return {};
			}

			CORE_USER_LOG(
				"Could not use compiled standard library binaries from the custom directory: "
				"some files are missing.\n"
			);
			art_collection = stdlib_art_collection;
		}

		std::vector<PackageCompilationTask> tasks;
		for (const auto& std_id: frontend::packages::standardLibraryPackageIds()) {
			auto pkg = std::find_if(
				global_state::getPackages().begin(),
				global_state::getPackages().end(),
				[&](const auto& pkg_info) { return pkg_info.getPackageID() == std_id; }
			);
			// If pkg is not loaded then we skip it.
			if (pkg == global_state::getPackages().end()) continue;

			if (global_state::getBackendOptions()->llvm_backend.has_value())
				tasks.emplace_back(
					pkg->getRootModule().illegalAccess().getID(),
					BuildTargetLLVMStaticLibrary{ .output_file_name
				                                  = base::StrID(base::strConcat(std_id, ".a")),
				                                  .archiving_options     = {},
				                                  .custom_art_collection = art_collection }
				);
			tasks.emplace_back(
				pkg->getRootModule().illegalAccess().getID(),
				BuildTargetDVMLibrary{ .output_file_name
			                           = base::StrID(base::strConcat(std_id, ".dbc")),
			                           .custom_art_collection = art_collection }
			);
		}
		return tasks;
	}

	base::Optional<std::string> getNativeStdLibLinkingArgs(
		const options_types::StdLibOptions& linking_options
	) {
		if_opt_some(resolveStdPath(linking_options), _) {
			std::string result;
			for (const auto& art: getStdLibNativeArtifacts())
				result += " " + art.file.getFilePath().string();
			return result;
		}
		return {};
	}

	namespace {
		/**
		 * @brief Result of collecting standard library artifacts for an extension.
		 */
		struct StdLibArtifacts {
			std::vector<artifacts::FileArtifact> artifacts;
			/// Whether an artifact was found for every standard library package.
			bool all_present;
		};

		/**
		 * @brief Small helper that gets the standard library artifacts from the
		 * root collection based on the provided extension.
		 */
		StdLibArtifacts getStdLibArtifacts(std::string_view extension) {
			std::vector<artifacts::FileArtifact>     artifacts;
			base::Ref<artifacts::ArtifactCollection> root_collection
				= global_state::getRootCollection();
			if_opt_some(global_state::getStdArtifactsCollection(), std_art_collection) {
				root_collection = std_art_collection;
			}
			bool all_present = true;

			for (const auto& std_id: frontend::packages::standardLibraryPackageIds()) {
				auto opt_artifact = root_collection->fileArtifactAtMaybe(
					base::StrID(base::strConcat(std_id, extension))
				);
				if_opt_some(opt_artifact, art) { artifacts.push_back(*art); }
				if_opt_none(opt_artifact) { all_present = false; }
			}
			return { .artifacts = std::move(artifacts), .all_present = all_present };
		}
	}

	std::vector<artifacts::FileArtifact> getStdLibNativeArtifacts() {
		return getStdLibArtifacts(".a").artifacts;
	}

	std::vector<artifacts::FileArtifact> getStdLibDVMArtifacts() {
		return getStdLibArtifacts(".dbc").artifacts;
	}

	std::vector<artifacts::FileArtifact> getStdLibDVMDebugInfoArtifacts() {
		return getStdLibArtifacts(".di.json").artifacts;
	}

	bool allStdlibArtifactsPresent() {
		bool native = getStdLibArtifacts(".a").all_present;
		bool dvm    = getStdLibArtifacts(".dbc").all_present;
		bool di     = getStdLibArtifacts(".di.json").all_present;
		return native && dvm && di;
	}
}
