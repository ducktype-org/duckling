#include "packages.hpp"


namespace global_state {

	namespace {
		std::vector<PackageInfo> packages;
		bool                     has_main_package = false;
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

	const PackageInfo& getMainPackage() {
		CORE_ASSERT(has_main_package, "No packages have been registered, main package is missing!");
		return packages.front();
	}

	namespace setters {
		void addPackage(compiler::frontend::ModuleID root_module) {
			packages.push_back({ root_module });
		}

		void removePackage(compiler::frontend::ModuleID root_module) {
			if (has_main_package && packages.front().root_module == root_module)
				has_main_package = false;
			std::erase_if(packages, [&](const PackageInfo& pkg) {
				return pkg.root_module == root_module;
			});
		}

		void addMainPackage(compiler::frontend::ModuleID root_module) {
			CORE_ASSERT(!has_main_package, "Main package has already been added!");
			packages.insert(packages.begin(), { root_module });
			has_main_package = true;
		}
	}
}
