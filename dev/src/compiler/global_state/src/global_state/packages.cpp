#include "packages.hpp"

#include <frontend/module_tree/functors.hpp>

#include <base/collections/optional.hpp>

namespace global_state {

	namespace {
		std::vector<PackageInfo> packages;
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

	base::CRef<global_state::PackageInfo> getPackageRef(base::StrID package_id) {
		for (const auto& pkg: packages)
			if (getModuleRef(pkg.root_module)->getPackageID() == package_id) return &pkg;
		CORE_PANIC("Package with ID ", package_id, " not found in global state!");
		CORE_UNREACHABLE();
	}

	base::Optional<base::CRef<global_state::PackageInfo>> getPackageRefOpt(base::StrID package_id) {
		for (const auto& pkg: packages)
			if (getModuleRef(pkg.root_module)->getPackageID() == package_id) return &pkg;
		return {};
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
