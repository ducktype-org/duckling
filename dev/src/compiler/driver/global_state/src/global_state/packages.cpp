#include "packages.hpp"

#include <base/misc/raw_view.hpp>
#include <base/str/string_id.hpp>

#include <string_view>

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
		void addPackage(std::string_view name, const fs::FilePath& path) {
			packages.push_back(
				{ base::StrID(base::RawView(reinterpret_cast<const byte*>(name.data()), name.size())
			      ),
			      path }
			);
		}

		void addMainPackage(std::string_view name, const fs::FilePath& path) {
			CORE_ASSERT(!main_package_set, "Main package has already been added!");
			packages.insert(
				packages.begin(),
				{ base::StrID(base::RawView(reinterpret_cast<const byte*>(name.data()), name.size())
			      ),
			      path }
			);
			main_package_set = true;
		}
	}
}
