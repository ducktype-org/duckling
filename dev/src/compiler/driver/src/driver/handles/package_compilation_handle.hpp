#pragma once

#include "handle_abc.hpp"

namespace driver {
    struct PackageCompilationHandle final: public CompilerHandleABC {
        /**
		 * Compile all package modules and link them into a single binary.
		 */
		void compilerEntirePackageIntoBinary();
    };
}

