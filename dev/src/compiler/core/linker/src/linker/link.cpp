
#include "link.hpp"

#include <time_stats/time_stats.hpp>

#include <logger/logger.hpp>
#include <system_command/system_command.hpp>

namespace compiler::linker {

	base::OkBad link(
		const artifacts::FileArtifact&              output,
		const std::vector<artifacts::FileArtifact>& inputs,
		const LinkingOptions&                       options
	) {
		time_stats::TrackCategoryTime linking_time(time_stats::TimeCategories::Linking);

		// Link the object file.
		// Use the default system linker - for Ubuntu it is advised to use gcc.
		// Related research links:
		// https://www.reddit.com/r/ProgrammingLanguages/comments/kji3k3/comment/ggx1ftq/
		// https://github.com/rust-lang/rust/issues/71519
		// https://github.com/rust-lang/rust/blob/c62239aeb3ba7781a6d7f7055523c1e8c22b409c/compiler/rustc_codegen_ssa/src/back/link.rs#L1442
		system_command::SystemCommand command("gcc");

		for (const auto& object_file_path: inputs)
			command.addArg(object_file_path.file.getFilePath().native());

		for (const auto& link_path: options.external_static_libraries)
			command.addArg(link_path.native());

		if (options.link_c_standard_library) command.addArg("-lc");  // Link the C standard library.

		command.addArg("-o");
		command.addArg(output.file.getFilePath().native());

		CORE_USER_LOG("Linking executable: ", output.file.getFilePath().name(), "\n");

		auto exit_code = command.execute(system_command::SystemCommand::ExitCodeHandling::Warn);

		return exit_code == 0 ? base::OK : base::BAD;
	}
}
