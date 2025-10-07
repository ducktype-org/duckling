#include "initialize.hpp"

#include <global_state/artifacts_location.hpp>
#include <global_state/options.hpp>
#include <linker/link.hpp>

#include <base/variant.hpp>

#include <diagnostic/logger.hpp>
#include <lexer/lexer_class.hpp>

namespace compiler::driver {

	namespace {
		constinit bool is_initialized = false;

		void handleDebugOptions(const options_types::DebugOptions& debug_options) {
			dia::Logger::setImmediatelyDump(debug_options.logger_cerr);
			lexer::Lexer::setTokenMessages(debug_options.lexer_cerr);
			global_state::getDynamicDebugOptions()->llvm_dump_ir  = debug_options.dump_llvm_ir;
			global_state::getDynamicDebugOptions()->llvm_dump_asm = debug_options.dump_llvm_asm;
		}

		void handleArtifactsOptions(const options_types::ArtifactsOptions& artifacts_options) {
			auto path = artifacts_options.artifacts_path;
			if (not path.exists()) {
				if (path.isPhysical()) {
					auto file = fs::FileManager::createPhysicalFolder(path);
					CORE_ASSERT(
						file.exists(), "Failed to create artifacts folder: " + path.string()
					);
				} else if (path.isTemporary()) {
					auto file = fs::FileManager::createTempFolder(path);
					CORE_ASSERT(
						file.exists(), "Failed to create artifacts folder: " + path.string()
					);
				} else {
					throw base::LogicError(
						"Artifacts path must be either physical or temporary, but got: "
						+ path.string()
					);
				}
			}
			global_state::setters::setRootCollection(
				makeBox<artifacts::ArtifactCollection>(artifacts_options.artifacts_path.getPath())
			);
		}

		void handleLinkingOptions(const linker::LinkingOptions& linking_options) {
			// Convert driver linking options to global state linking options
			linker::setLinkingOptions(linking_options);
		}
	}

	void initializeTheCompiler(CompilerModeOfOperationAndOptions options) {
		CORE_ASSERT(
			init::wasInitObject(),
			"InitObject should be used before call to the initializeTheCompiler function!"
		);

		CORE_ASSERT(!is_initialized, "Compiler is already initialized!");
		is_initialized = true;

		variant_match(options.mode) {
			variant_case(CompilerModeOfOperationAndOptions::BareMode, options) {
				handleDebugOptions(options.debug_options);
			}
			variant_case(CompilerModeOfOperationAndOptions::PackageCompilationMode, options) {
				handleDebugOptions(options.debug_options);
				handleArtifactsOptions(options.compilation_artifacts);
				handleLinkingOptions(options.linking_options);
			}

			variant_default { CORE_PANIC("Unknown compiler mode of operation"); }
		}
	}
}
