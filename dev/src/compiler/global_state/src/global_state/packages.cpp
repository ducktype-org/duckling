#include "packages.hpp"

#include <base/collections/optional.hpp>

namespace global_state {

	namespace {
		std::vector<PackageInfo> packages;
		// Tracks whether a main package has been registered already
		base::Optional<Ref<PackageInfo>> main_package{};
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

	const PackageInfo& getMainPackage() {
		CORE_ASSERT(
			!main_package.empty(), "No packages have been registered, main package is missing!"
		);
		return *main_package.value().get();
	}

	namespace setters {
		void addPackage(compiler::frontend::ModuleID root_module) {
			packages.push_back({ root_module });
		}

		void removePackage(compiler::frontend::ModuleID root_module) {
			if_opt_some(main_package, main_pkg) {
				if (main_pkg->root_module == root_module) main_package.reset();
			}
			std::erase_if(packages, [&](const PackageInfo& pkg) {
				return pkg.root_module == root_module;
			});
		}

		void addMainPackage(compiler::frontend::ModuleID root_module) {
			CORE_ASSERT(main_package.empty(), "Main package has already been added!");
			packages.insert(packages.begin(), { root_module });
			main_package.emplace(&packages.front());
		}
	}
}
