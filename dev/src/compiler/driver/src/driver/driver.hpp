#pragma once

#include "options.hpp"
#include <base/box.hpp>
#include "handles/handle_abc.hpp"

namespace driver {

    /**
     * @brief Initializes the compiler with the given options. 
     */
    Box<CompilerHandleABC> initializeTheCompiler(
        CompilerModeOfOperationAndOptions options
    );
}
