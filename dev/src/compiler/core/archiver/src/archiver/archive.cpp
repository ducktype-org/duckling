// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "archive.hpp"

#include <global_state/global_logger.hpp>
#include <time_stats/time_stats.hpp>

#include <base/config/target_info.hpp>

#include <diagnostic/logger.hpp>
#include <diagnostic/placeholder.hpp>
#include <filesystem/file.hpp>
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

		// `ar rcs <archive> <objs...>` — standard POSIX archive tool invocation.
		// r: insert with replace, c: silently create missing archive, s: write symbol index.
		system_command::SystemCommand command(options.archiver_path.copyValueOr("ar"));
		command.addArg("rcs");

#if BASE_TARGET_OS_MACOS
		// Apple's cctools `ar` stamps each member with its mtime and the current uid/gid, so
		// re-archiving the same objects yields different bytes and every consumer of the archive
		// hash sees a spurious change. It has no deterministic flag (`-D` is rejected); the
		// cctools mechanism is this environment variable, which zeroes all three.
		command.addEnv("ZERO_AR_DATE", "1");
#endif

		CORE_USER_LOG("Archiving static library: ", output.path.name(), "\n");

		// `ar` writes a fresh temporary archive that is renamed into place. A fresh file is what
		// keeps `ar r` from updating members of a previous archive - it used to be deleted first
		// for that - and the rename means another build sharing the folder never reads a
		// half-written archive, while a failed `ar` leaves the previous one intact.
		bool archiver_failed = false;
		auto produced        = fs::FileManager::produceFileAtomically(
            output.path,
            [&](const fs::FilePath& temp_path) -> std::expected<void, std::string> {
                command.addArg(temp_path.native());
                for (const auto& object_file_path: inputs)
                    command.addArg(object_file_path.path.native());
                const auto exit_code
                    = command.execute(system_command::SystemCommand::ExitCodeHandling::Warn);
                if (exit_code == 0) return {};
                archiver_failed = true;
                return std::unexpected("the archiver failed");
            }
        );
		if (!produced.has_value() && !archiver_failed)
			CORE_USER_LOG("Error: could not put the archive in place: ", produced.error(), "\n");
		auto result = produced.has_value() ? base::OK : base::BAD;

		if (archiver_failed) {
			if ((not query::Context::areWeInsideQuery()) and global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(makeBox<dia::PlaceholderError>(
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
