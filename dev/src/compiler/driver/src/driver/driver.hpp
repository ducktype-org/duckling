#pragma once

#include "options.hpp"
#include <base/box.hpp>

namespace driver {
    
    struct CompilerHandle {
        virtual ~CompilerHandle() = default;
    };

    Box<CompilerHandle> initializeTheCompiler(
        CompilerModeOfOperationAndOptions options
    );
}
