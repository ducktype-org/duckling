#include "packages.hpp"

namespace global_state {

	namespace {
		std::vector<PackageInfo> packages;
		// Tracks whether a main package has been registered already
		constinit bool main_package_set = false;
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

	const PackageInfo& getMainPackage() {
		CORE_ASSERT(!packages.empty(), "No packages have been registered, main package is missing!");
		return packages.front();
	}

	namespace setters {
		void addPackage(compiler::frontend::ModuleID root_module) {
			packages.push_back({ root_module });
		}

		void addMainPackage(compiler::frontend::ModuleID root_module) {
			CORE_ASSERT(!main_package_set, "Main package has already been added!");
			packages.insert(packages.begin(), { root_module });
			main_package_set = true;
		}
	}
}
