#include "packages.hpp"

#include <frontend/module_tree/functors.hpp>

#include <algorithm>

namespace global_state {

	namespace {
		std::vector<PackageInfo> packages;
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

	base::Optional<PackageInfo> getPackageInfoForRootModule(compiler::frontend::ModuleID module_id) {
		CORE_ASSERT(
			!compiler::frontend::getModuleRef(module_id)->getParentModule().has_value(),
			"Expected root package module in getPackageInfoForRootModule, but module has parent"
		);

		if (packages.empty()) return {};

		auto module_package_id = compiler::frontend::getModuleRef(module_id)->getPackageID();
		for (const auto& package_info: packages) {
			auto package_id
				= compiler::frontend::getModuleRef(package_info.root_module)->getPackageID();
			if (package_id == module_package_id) return package_info;
		}

		return {};
	}

	std::vector<compiler::frontend::ModuleID> getAllPackagesWithDependenciesRootModulesSortedDeduplicated(
	) {
		std::vector<compiler::frontend::ModuleID> module_ids;
		for (const auto& package_info: packages) {
			module_ids.push_back(package_info.root_module);
			module_ids.insert(
				module_ids.end(), package_info.dependencies.begin(), package_info.dependencies.end()
			);
		}

		std::ranges::sort(module_ids, [](auto lhs, auto rhs) {
			return lhs.queryUnstablePerfectHash() < rhs.queryUnstablePerfectHash();
		});
		auto tail = std::ranges::unique(module_ids, [](auto lhs, auto rhs) {
			return lhs.queryUnstablePerfectHash() == rhs.queryUnstablePerfectHash();
		});
		module_ids.erase(tail.begin(), tail.end());
		return module_ids;
	}

	namespace setters {
		void addPackage(const PackageInfo& package_info) { packages.push_back(package_info); }

		void addPackage(compiler::frontend::ModuleID root_module) {
			packages.push_back({ .root_module = root_module, .dependencies = {} });
		}

		void removePackage(compiler::frontend::ModuleID root_module) {
			std::erase_if(packages, [&](const PackageInfo& pkg) {
				return pkg.root_module == root_module;
			});
		}
	}
}
