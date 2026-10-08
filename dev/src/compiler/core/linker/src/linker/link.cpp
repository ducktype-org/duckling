// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "link.hpp"

#include <global_state/global_logger.hpp>
#include <time_stats/time_stats.hpp>

#include <diagnostic/logger.hpp>
#include <diagnostic/placeholder.hpp>
#include <logger/logger.hpp>
#include <query_framework/context/context.hpp>
#include <system_command/system_command.hpp>

namespace compiler::linker {

	base::OkBad linkExecutable(
		const artifacts::FileArtifact&              output,
		const std::vector<artifacts::FileArtifact>& inputs,
		const LinkingOptions&                       options
	) {
		time_stats::TrackCategoryTime linking_time(time_stats::TimeCategories::Linking);

		// Link the object file.
		// Use the default system linker if the linker is not set - for Ubuntu it is advised to use
		// gcc. Related research links:
		// https://www.reddit.com/r/ProgrammingLanguages/comments/kji3k3/comment/ggx1ftq/
		// https://github.com/rust-lang/rust/issues/71519
		// https://github.com/rust-lang/rust/blob/c62239aeb3ba7781a6d7f7055523c1e8c22b409c/compiler/rustc_codegen_ssa/src/back/link.rs#L1442
		system_command::SystemCommand command(options.linker_path.copyValueOr("gcc"));

		for (const auto& object_file_path: inputs)
			command.addArg(object_file_path.file.getFilePath().native());

		command.addArgs(options.additional_link_options);
		command.addArgs(options.stdlib_link_options);  // Link the Duckling standard library.
		if (options.link_c_standard_library) {
			command.addArg("-lc");                     // Link the C standard library.
			command.addArg("-lm");                     // Link the C math library.
		}

		command.addArg("-o");
		command.addArg(output.file.getFilePath().native());

		CORE_USER_LOG("Linking executable: ", output.file.getFilePath().name(), "\n");

		auto exit_code = command.execute(system_command::SystemCommand::ExitCodeHandling::Warn);
		auto linking_result = exit_code == 0 ? base::OK : base::BAD;

		if (linking_result.isBad()) {
			if ((not query::Context::areWeInsideQuery()) and global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(makeBox<dia::PlaceholderError>(
					"Linking of the final executable failed. See the linker output above. ",
					"The common reasons for this error may include missing main function "
					"(temporary "
					"feature), missing linker options related to external libraries or duplicated "
					"declaration not detected by the compiler."
				));
			} else {
				CORE_USER_LOG(
					"\nWARNING: linker called inside a query or the global logger is not "
					"available. Falling back to the user logs for diagnostics.\n"
				);
				CORE_USER_LOG(
					"\nERROR: Linking of the final executable failed. See the linker output "
					"above. ",
					"The common reasons for this error may include missing main function "
					"(temporary "
					"feature), missing linker options related to external libraries or duplicated "
					"declaration not detected by the compiler.\n"
				);
			}
		}

		return linking_result;
	}
}
