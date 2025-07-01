#pragma once

#include "options.hpp"
#include <base/box.hpp>
#include "handles/handle_abc.hpp"

namespace driver {
    
    

    Box<CompilerHandleABC> initializeTheCompiler(
        CompilerModeOfOperationAndOptions options
    );
}
