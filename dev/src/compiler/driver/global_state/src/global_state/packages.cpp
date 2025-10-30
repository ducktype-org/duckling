#include "packages.hpp"

#include <base/str/string_id.hpp>

namespace global_state {

	static std::vector<PackageInfo> packages;

	const std::vector<PackageInfo>& getPackages() { return packages; }

	const PackageInfo& getMainPackage() {
		CORE_ASSERT(!packages.empty(), "No packages have been registered, main package is missing!");
		return packages.front();
	}

	namespace setters {
		void addPackage(const std::string& name, const fs::FilePath& path) {
			packages.push_back({ base::StrID(name.c_str()), path });
		}

		void addMainPackage(const std::string& name, const fs::FilePath& path) {
			packages.insert(packages.begin(), { base::StrID(name.c_str()), path });
		}

		void clearPackagesForTests() { packages.clear(); }
	}
}
