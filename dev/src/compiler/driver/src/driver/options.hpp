#pragma once

#include <base/ints.hpp>
#include <variant>
#include <string>
#include <vector>

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

        struct ArtifactsOptions {
            struct IncrementalCompilation {
                bool enabled;
                bool show_stats;
            };
            std::string artifacts_path;
            IncrementalCompilation incremental_compilation;
            bool rm_artifacts_before_compilation = false;
            bool rm_artifacts_after_compilation = false;
        };

        struct PackageInfo {
            std::string package_name;
            std::string package_path;
        };

        struct DependencyInfo {
            struct CompilationStrategy {
                struct InlineCompilation { };
                struct Precompiled {
                    // this might be inlined or not:
                    std::string precompilation_path;
                };
                std::variant<InlineCompilation, Precompiled> strategy;
            };
            PackageInfo package_info;
            CompilationStrategy compilation_strategy;
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

        /**
         * Bare mode, where compiler doesn't do any initialization etc,
         * but you can still (try to) use its internal functions.
         */
        struct BareMode {
            // anything here?
        };

        /**
         * Package compilation mode, compiler is used to compile a package
         * and its dependencies it can be later asked to compile specific scripts
         * or modules, etc. 
         */
        struct PackageCompilationMode {
            options_types::PackageInfo main_package_info;
            options_types::ArtifactsOptions compilation_artifacts;
            std::vector<options_types::DependencyInfo> dependencies;
            options_types::CompilationOptions compilation_options;
            options_types::DebugOptions debug_options;
        };
 
        /**
         * @note: in the future this might hold more modes,
         * like repl mode, script compilation mode, lsp deamon, etc.
         * don't refrain from refactoring this file (and module) if needed.
         */
        std::variant<BareMode, PackageCompilationMode> mode;
    };
};
