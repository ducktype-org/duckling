
#include "link.hpp"

#include <system_command/system_command.hpp>

namespace compiler::linker {

	void link(
		const artifacts::FileArtifact& output, const std::vector<artifacts::FileArtifact>& inputs
	) {
		auto& options = getLinkingOptions();

		// Link the object file.
		// Use the default system linker - for Ubuntu it is advised to use gcc.
		// Related research links:
		// https://www.reddit.com/r/ProgrammingLanguages/comments/kji3k3/comment/ggx1ftq/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1
		// https://github.com/rust-lang/rust/issues/71519
		// https://github.com/rust-lang/rust/blob/c62239aeb3ba7781a6d7f7055523c1e8c22b409c/compiler/rustc_codegen_ssa/src/back/link.rs#L1442
		system_command::SystemCommand command("gcc");

		for (const auto& object_file_path: inputs)
			command.addArg(object_file_path.FILE.getFilePath().native());

		for (const auto& link_path: options.external_static_libraries)
			command.addArg(link_path.native());

		if (options.link_c_standard_library) command.addArg("-lc");  // Link the C standard library.


		command.addArg("-o");
		command.addArg(output.FILE.getFilePath().native());
		command.execute();
	}

	static LinkingOptions linking_options{};
	static bool           linking_options_initialized = false;

	const LinkingOptions& getLinkingOptions() {
		CORE_ASSERT(
			linking_options_initialized,
			"Linking options not initialized! Call setLinkingOptions during compiler "
			"initialization."
		);
		return linking_options;
	}

	void setLinkingOptions(const LinkingOptions& options) {
		linking_options             = options;
		linking_options_initialized = true;
	}
}
