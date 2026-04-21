#include "archive.hpp"

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <time_stats/time_stats.hpp>

#include <logger/logger.hpp>
#include <query_framework/context/context.hpp>
#include <system_command/system_command.hpp>

namespace compiler::archiver {
	base::OkBad createArchive(
		const artifacts::FileArtifact&              output,
		const std::vector<artifacts::FileArtifact>& inputs,
		const ArchivingOptions&                     options
	) {
		time_stats::TrackCategoryTime linking_time(time_stats::TimeCategories::Linking);

		// Artifact Manager can pre-create an empty file for this artifact path.
		// Also remove any existing .a so `ar rcs` builds a clean archive from current objects,
		// instead of updating/reusing members from a previous archive.
		if (output.file.exists()) {
			auto deleted = fs::FileManager::deleteFile(output.file);
			if (!deleted && output.file.exists()) {
				if ((not query::Context::areWeInsideQuery()) and global_state::hasGlobalLogger()) {
					global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderHeaderError>(
						"Archiving of the static library failed.",
						"Could not remove existing output file before invoking the archiver. "
						"Check filesystem permissions for the artifacts directory."
					));
				} else {
					CORE_USER_LOG(
						"\nERROR: Could not remove existing output file before invoking the archiver. "
						"Check filesystem permissions for the artifacts directory.\n"
					);
				}
				return base::BAD;
			}
		}

		// `ar rcs <archive> <objs...>` — standard POSIX archive tool invocation.
		// r: insert with replace, c: silently create missing archive, s: write symbol index.
		system_command::SystemCommand command(options.archiver_path.copyValueOr("ar"));
		command.addArg("rcs");
		command.addArg(output.file.getFilePath().native());

		for (const auto& object_file_path: inputs)
			command.addArg(object_file_path.file.getFilePath().native());

		CORE_USER_LOG("Archiving static library: ", output.file.getFilePath().name(), "\n");

		auto exit_code = command.execute(system_command::SystemCommand::ExitCodeHandling::Warn);
		auto result    = exit_code == 0 ? base::OK : base::BAD;

		if (result.isBad()) {
			if ((not query::Context::areWeInsideQuery()) and global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderHeaderError>(
					"Archiving of the static library failed. See the archiver output above.",
					"Check that the selected archiver is installed and accessible via PATH."
				));
			} else {
				CORE_USER_LOG(
					"\nERROR: Archiving of the static library failed. See the archiver output "
					"above.\n"
				);
			}
		}

		return result;
	}
}