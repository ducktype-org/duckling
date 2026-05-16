
#include "link.hpp"

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <time_stats/time_stats.hpp>

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

		command.addArg(options.additional_link_options);

		if (options.link_c_standard_library)
			command.addArg("-lc");  // Link the C standard library.<
		if_opt_some(options.stdlib_link_options, stdlib_link_options)
			command.addArg(stdlib_link_options);

		command.addArg("-o");
		command.addArg(output.file.getFilePath().native());

		CORE_USER_LOG("Linking executable: ", output.file.getFilePath().name(), "\n");

		auto exit_code = command.execute(system_command::SystemCommand::ExitCodeHandling::Warn);
		auto linking_result = exit_code == 0 ? base::OK : base::BAD;

		if (linking_result.isBad()) {
			if ((not query::Context::areWeInsideQuery()) and global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
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
