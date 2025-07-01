#pragma once

namespace driver {
    /**
     * Top level api of the compiler.
     * The concrete object depends on the mode of operations
     * provided to the initializeTheCompiler function.
     */
    struct CompilerHandleABC {
        virtual ~CompilerHandleABC() = default;
    };
}
