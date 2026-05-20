#include "standard_library.hpp"

#include <driver_private/standard_library/standard_library.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ok_bad.hpp>

#include <algorithm>

namespace compiler::driver {
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
		const options_types::GlobalLinkingOptions& standard_library_options
	) {
		base::Optional<fs::FilePath> path;
		variant_match(standard_library_options.std_lib_type) {
			variant_case(options_types::GlobalLinkingOptions::NoStd, _) { return {}; }
			variant_case(options_types::GlobalLinkingOptions::CustomStd, custom_std) {
				path = custom_std.std_path;
			}
			variant_case(options_types::GlobalLinkingOptions::DefaultStd, _) {
				path = resolveDefaultStdPath();
			}
			variant_default { CORE_UNREACHABLE(); }
		}
		return path;
	}

	base::OkBad addStandardLibraryPackages(
		const fs::FilePath& std_path, frontend::packages::DiagnosticReporter& report
	) {
		using compiler::frontend::packages::RawDependencyInfo;
		using compiler::frontend::packages::RawPackageInfo;

		for (auto& config: STD_PACKAGES_CONFIG) {
			auto package_path = std_path.join(fs::FilePath(config.subpath));
			if (not package_path.exists()) {
				report(
					"Standard library package path does not exist.",
					base::strConcat("Expected standard library package at: ", package_path.string()),
					true
				);
				return base::BAD;
			}
			RawPackageInfo raw_package_info{
				.package_name = base::StrID(config.name),
				.version      = base::StrID("not_supported"),
				.package_path = package_path,
				.features     = {},
				.dependencies = {},
			};

			for (const auto& dep_name: config.dependencies) {
				raw_package_info.dependencies.push_back(RawDependencyInfo{
					.package_name = base::StrID(dep_name),
					.alias        = {},
				});
			}

			auto package_info
				= compiler::frontend::packages::createPackageInfo(raw_package_info, report);
			if (!package_info.has_value()) return base::BAD;
			global_state::setters::addPackage(*package_info);
		}
		return base::OK;
	}

	namespace {
		bool hasDependency(
			const std::vector<compiler::frontend::packages::RawDependencyInfo>& dependencies,
			base::StrID                                                         target_package
		) {
			return std::ranges::any_of(dependencies, [&](const auto& dep) {
				return dep.package_name == target_package;
			});
		}

		base::OkBad addStandardLibraryDependenciesForPackage(
			compiler::frontend::packages::RawPackageInfo& package_info,
			frontend::packages::DiagnosticReporter&       report
		) {
			for (auto& config: STD_PACKAGES_CONFIG) {
				if (hasDependency(package_info.dependencies, base::StrID(config.name))) {
					report(
						"Standard library package name is already used as an external dependency.",
						base::strConcat(
							"Package '", package_info.package_name, "' depends on '", config.name
						),
						true
					);
					return base::BAD;
				}
				package_info.dependencies.push_back(compiler::frontend::packages::RawDependencyInfo{
					.package_name = base::StrID(config.name),
					.alias        = {},
				});
			}
			return base::OK;
		}
	}

	base::OkBad addStandardLibraryDependencies(
		std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
		frontend::packages::DiagnosticReporter&                    report

	) {
		for (auto& package_info: packages_info)
			if (addStandardLibraryDependenciesForPackage(package_info, report).isBad())
				return base::BAD;
		return base::OK;
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

			tasks.push_back(PackageCompilationTask{
				.root_module = pkg->getRootModule().illegalAccess().getID(),
				.build_target
				= BuildTargetLLVMStaticLibrary{ .output_file_stem  = base::StrID(config.name),
			                                    .archiving_options = {} },
			});
			tasks.push_back(PackageCompilationTask{
				.root_module  = pkg->getRootModule().illegalAccess().getID(),
				.build_target = BuildTargetDVM{ .output_file_stem = base::StrID(config.name) },
			});
		}
		return tasks;
	}

	base::Optional<std::string> getStdLibLinkingArgs(
		const options_types::GlobalLinkingOptions& linking_options
	) {
		if_opt_some(resolveStdPath(linking_options), _) {
			// Temporary directory for the `.a` files for the standard library is just a root
			// collection, where every ".a" file is located.
			std::string result = "-L" + getStdBinariesDirectory().string();
			for (const auto& config: STD_PACKAGES_CONFIG)
				result += " -l:" + std::string(config.name) + ".a";
			return result;
		}
		return {};
	}

	fs::FilePath getStdBinariesDirectory() {
		return global_state::getRootCollection()->getDirectoryPath();
	}
}
