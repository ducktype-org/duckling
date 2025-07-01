#include "driver.hpp"

#include <base/variant.hpp>
#include "handles/bare_handle.hpp"
#include "handles/package_compilation_handle.hpp"
#include <global_state/artifacts_location.hpp>

#include <diagnostic/logger.hpp>
#include <lexer/lexer_class.hpp>

namespace driver {

    namespace {
        constinit bool is_initialized = false;

        void handleDebugOptions(const options_types::DebugOptions& debug_options) {
            dia::Logger::setImmediatelyDump(debug_options.logger_cerr);
		    lexer::Lexer::setTokenMessages(debug_options.lexer_cerr);
        }
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
            variant_case(CompilerModeOfOperationAndOptions::PackageCompilationMode, options) {
                global_state::setters::setRootCollection(
                    makeBox<artifacts::ArtifactCollection>(options.compilation_artifacts.artifacts_path)
                );

                handleDebugOptions(options.debug_options);

                return makeBox<PackageCompilationHandle>();
            }

            variant_default {
                CORE_PANIC("Unknown compiler mode of operation");
            }
        }
        CORE_UNREACHABLE();
    }
}
