#include "packages.hpp"

#include <frontend/module_tree/functors.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <artifacts/artifacts.hpp>

namespace global_state {

	using compiler::frontend::packages::PackageInfo;

	namespace {
		std::vector<PackageInfo>                           packages;
		base::Optional<Box<artifacts::ArtifactCollection>> std_art_collection;
	}

	const std::vector<PackageInfo>& getPackages() { return packages; }

	base::Optional<base::Ref<artifacts::ArtifactCollection>> getStdArtifactsCollection() {
		return std_art_collection.map([](auto& box) { return box.refMut(); });
	}

	base::CRef<PackageInfo> getPackageRef(base::StrID package_id) {
		for (const auto& pkg: packages)
			if (pkg.getPackageID() == package_id) return &pkg;
		CORE_PANIC("Package with ID ", package_id, " not found in global state!");
		CORE_UNREACHABLE();
	}

	base::Optional<base::CRef<PackageInfo>> getPackageRefOpt(base::StrID package_id) {
		for (const auto& pkg: packages)
			if (pkg.getPackageID() == package_id) return &pkg;
		return {};
	}

	namespace setters {
		void addPackage(const PackageInfo& package_info) { packages.push_back(package_info); }

		void addPackage(compiler::frontend::ModuleID root_module) {
			packages.emplace_back(
				root_module,
				compiler::frontend::getModuleRef(root_module)->getPackage().illegalAccess().getID(),
				base::StrID("not_supported"),
				std::vector<base::StrID>{},
				std::vector<compiler::frontend::packages::PackageDependencyInfo>{}
			);
		}

		void removePackage(compiler::frontend::ModuleID root_module) {
			std::erase_if(packages, [&](const PackageInfo& pkg) {
				return pkg.getRootModule().illegalAccess().getID() == root_module;
			});
		}

		void setCustomStdArtifactsCollection(
			base::Box<artifacts::ArtifactCollection> custom_art_collection
		) {
			std_art_collection = std::move(custom_art_collection);
		}
	}

}  // namespace global_state
