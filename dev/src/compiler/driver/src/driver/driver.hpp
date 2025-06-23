#pragma once

#include "options.hpp"

namespace driver {

    /**
     * Class that encapsulates all high-level compiler logic
     * and modes of operation.
     * This is expected to be a singleton, but is kept as a class
     * for better code organisation.
     */
    class Driver {

    public:

        static void initializeCompiler(
            CompilerModeOfOperationAndOptions options
        );




    };
}