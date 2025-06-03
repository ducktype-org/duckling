#pragma once

#include "backend_driver/backend_options.hpp"
#include <artifacts/artifacts.hpp>

namespace compiler::driver {

    /**
     * Driver that automates process of package compilation,
     * handles package artifacts and encapsulates some query operations
     * into high-level cacheable operations.
     */
    class PackageCompilationDriver final {
        BackendOptions options;
        base::StrID    package_location;
        artifacts::ArtifactCollection root_artifacts;

    public:
        PackageCompilationDriver() = delete;
        PackageCompilationDriver(PackageCompilationDriver&&) = delete;
        PackageCompilationDriver(const PackageCompilationDriver&) = delete;

        PackageCompilationDriver(
            BackendType     backend,
            base::StrID     package_location,
            base::StrID     artifact_location
        );

        /**
         * Compile all package modules and link them into a single binary.
         */
        void compilerEntirePackageIntoBinary();
    };
}
