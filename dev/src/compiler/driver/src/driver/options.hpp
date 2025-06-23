#pragma once

#include <base/ints.hpp>
#include <variant>
#include <string>

namespace driver {    


    /**
     * Definition of options that are used by the compiler to control its behavior.
     */
    namespace options_types {
        /**
         * Options used for actual compilation of the source code.
         */
        struct CompilationOptions {
            struct OptimizationOptions {
                // @future: made it more specific
                u64 level;
            };

            struct BackendOptions {
                struct VMBackend {};
                struct LLVMBackend {};
                std::variant<VMBackend, LLVMBackend> backend_options;
            };

            CompilationOptions(OptimizationOptions opt, BackendOptions backend)
                : optimization(opt), backend(backend) {}


            OptimizationOptions optimization;
            BackendOptions backend;
        };

        struct DebugOptions {
            bool lexer_cerr = false;
            bool logger_cerr = false;
        };
    };


    /**
     * Structure holding information about mode of operation of the compiler
     * and all "generic" compiler options associated with that mode,
     * (i.e. options that are not associated with any specific task).
     * Note that is doesn't encapsulate cli options such as --help or --version.
     * It is used to configure the compiler's behavior once the compiler is actually used
     * via the Driver.
     */
    struct CompilerModeOfOperationAndOptions {
        struct BareMode {

        };
        struct PackageCompilation {
            
        };
        struct ScriptCompilation {

        };

        std::variant<BareMode, PackageCompilation, ScriptCompilation> mode;
    };



};