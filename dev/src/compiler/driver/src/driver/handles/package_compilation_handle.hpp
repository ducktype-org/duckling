#pragma once

#include "../handle_abc.hpp"
#include <filesystem/file.hpp>

namespace driver {
    class PackageCompilationHandle final: public CompilerHandleABC {
        fs::FilePath package_location;

    public:
        PackageCompilationHandle() = delete;
        PackageCompilationHandle(fs::FilePath package_location):
            package_location(std::move(package_location)) {}

        /**
		 * Compile all package modules and link them into a single binary.
		 */
		void compilerEntirePackageIntoBinary();
    };
}

