#include "test_utils.hpp"

#include <driver/initialize.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/packages/packages.hpp>
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>

#include <ranges>
#include <string>
#include <vector>

namespace compiler::driver::test_utils {
	namespace {
		compiler::frontend::ModuleID findSubmodule(
			compiler::frontend::ModuleID start_module,
			const std::vector<std::string>& path
		) {
			compiler::frontend::ModuleID current_module = start_module;
			for (const auto& part: path) {
				current_module = compiler::frontend::getModuleRef(current_module)
					                 ->getSubmoduleByName(base::StrID(part))
					                 .illegalAccess()
					                 ->illegalAccess()
					                 .getID();
			}
			return current_module;
		}
	}

	base::CheckedOkBad initializeCompilerForTests(
		const std::vector<PackagePathAndName>& packages,
		const fs::FilePath&                   artifacts_path
	) {
		options_types::StdLibOptions stdlib_options{
			.std_lib_type = options_types::StdLibOptions::DefaultStd{},
		};
		return initializeCompilerForTests(packages, artifacts_path, stdlib_options);
	}

	base::CheckedOkBad initializeCompilerForTests(
		const std::vector<PackagePathAndName>& packages,
		const fs::FilePath&                   artifacts_path,
		const options_types::StdLibOptions&   stdlib_options
	) {
		initializeGlobalLogger();

		std::vector<compiler::frontend::packages::RawPackageInfo> raw_packages;
		raw_packages.reserve(packages.size());

		for (const auto& [package_path, package_name]: packages) {
			raw_packages.push_back(compiler::frontend::packages::RawPackageInfo{
				.package_name = base::StrID(package_name),
				.version      = base::StrID("0.1.0"),
				.package_path = package_path,
				.features     = {},
				.dependencies = {},
			});
		}

		return initializeTheCompiler(CompilerModeOfOperationAndOptions::PackageCompilationMode{
			.packages_info = std::move(raw_packages),
			.compilation_artifacts = {
				.artifacts_path = artifacts_path,
			},
			.backend_options = {
				.llvm_backend = {},
			},
			.debug_options     = {},
			.incremental       = {},
			.execution_options = { .worker_count = 1 },
			.stdlib_options    = stdlib_options,
		});
	}

	compiler::frontend::ModuleID getModuleIdFromPath(std::string_view module_path) {
		std::vector<std::string> path_parts = std::string(module_path) | std::views::split('/')
		                                    | std::views::transform([](auto&& part) {
										return std::string(part.begin(), part.end());
									})
		                                    | std::ranges::to<std::vector>();

		CORE_ASSERT(!path_parts.empty(), "Module path must not be empty");

		auto                     package_name = base::StrID(path_parts.front());
		std::vector<std::string> submodule_path_parts(path_parts.begin() + 1, path_parts.end());
		base::Optional<compiler::frontend::ModuleID> root_module_id;
		for (const auto& pkg_info: global_state::getPackages()) {
			if (pkg_info.getPackageID() == package_name) {
				root_module_id = pkg_info.getRootModule().illegalAccess().getID();
				break;
			}
		}

		CORE_ASSERT(root_module_id.has_value(), "Package not found for module path");

		return findSubmodule(*root_module_id, submodule_path_parts);
	}
}
