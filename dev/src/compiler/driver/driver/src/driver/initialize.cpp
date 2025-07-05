#include "initialize.hpp"

#include <base/variant.hpp>
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

        void handleArtifactsOptions(
            const options_types::ArtifactsOptions& artifacts_options
        ) {
            global_state::setters::setRootCollection(
                makeBox<artifacts::ArtifactCollection>(artifacts_options.artifacts_path)
            );

        }
    }

    void initializeTheCompiler(
        CompilerModeOfOperationAndOptions options
    ) {
        // @TODO: maybe validate that init was done here
        
        CORE_ASSERT(!is_initialized, "Compiler is already initialized!");
        is_initialized = true;
        
        variant_match(options.mode) {
            variant_case(CompilerModeOfOperationAndOptions::BareMode, _) {
            }
            variant_case(CompilerModeOfOperationAndOptions::PackageCompilationMode, options) {
                handleDebugOptions(options.debug_options);
                handleArtifactsOptions(options.compilation_artifacts);

            }

            variant_default {
                CORE_PANIC("Unknown compiler mode of operation");
            }
        }
        CORE_UNREACHABLE();
    }
}
