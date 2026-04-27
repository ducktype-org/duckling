#include "packages.hpp"

#include <frontend/module_tree/functors.hpp>

#include <algorithm>

namespace global_state {

	namespace {
		std::vector<PackageInfo> packages;
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

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
