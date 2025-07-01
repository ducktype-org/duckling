#include "driver.hpp"

#include <base/variant.hpp>
#include "handles/bare_handle.hpp"
#include "handles/package_compilation_handle.hpp"

namespace driver {

    namespace {
        constinit bool is_initialized = false;
    }

    Box<CompilerHandleABC> initializeTheCompiler(
        CompilerModeOfOperationAndOptions options
    ) {
        // @TODO: maybe validate that init was done here
        
        CORE_ASSERT(!is_initialized, "Compiler is already initialized!");
        is_initialized = true;
        
        variant_match(options.mode) {
            variant_case(CompilerModeOfOperationAndOptions::BareMode, _) {
                return makeBox<BareHandle>();
            }
            variant_case(CompilerModeOfOperationAndOptions::PackageCompilationMode, _) {
                // @TODO: set stuff here
                return makeBox<PackageCompilationHandle>();
            }

            variant_default {
                CORE_PANIC("Unknown compiler mode of operation");
            }
        }
        CORE_UNREACHABLE();
    }
}
