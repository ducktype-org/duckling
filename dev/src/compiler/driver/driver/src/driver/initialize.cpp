#include "initialize.hpp"

#include <diagnostic/logger.hpp>
#include <global_state/artifacts_location.hpp>
#include <lexer/lexer_class.hpp>

#include <base/variant.hpp>

namespace compiler::driver {

	namespace {
		constinit bool is_initialized = false;

		void handleDebugOptions(const options_types::DebugOptions& debug_options) {
			dia::Logger::setImmediatelyDump(debug_options.logger_cerr);
			lexer::Lexer::setTokenMessages(debug_options.lexer_cerr);
		}

		void handleArtifactsOptions(const options_types::ArtifactsOptions& artifacts_options) {
			CORE_ASSERT(
				fs::FileManager::fileExists(artifacts_options.artifacts_path),
				"Artifacts path does not exist:", artifacts_options.artifacts_path.nativePath(), "!"
			);
			global_state::setters::setRootCollection(
				makeBox<artifacts::ArtifactCollection>(artifacts_options.artifacts_path.getPath())
			);
		}
	}

	void initializeTheCompiler(CompilerModeOfOperationAndOptions options) {
		// @TODO PR: maybe validate that init was done here

		CORE_ASSERT(!is_initialized, "Compiler is already initialized!");
		is_initialized = true;

		variant_match(options.mode) {
			variant_case(CompilerModeOfOperationAndOptions::BareMode, _) {}
			variant_case(CompilerModeOfOperationAndOptions::PackageCompilationMode, options) {
				handleDebugOptions(options.debug_options);
				handleArtifactsOptions(options.compilation_artifacts);
			}

			variant_default { CORE_PANIC("Unknown compiler mode of operation"); }
		}
		CORE_UNREACHABLE();
	}
}
