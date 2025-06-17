
#include "link.hpp"

#include <system_command/system_command.hpp>

namespace compiler::driver {

	void link(
		const artifacts::FileArtifact&              output,
		const std::vector<artifacts::FileArtifact>& inputs,
		LinkOptions                          options
	) {
		// Link the object file.
		// Use the default system linker - for Ubuntu it is advised to use gcc.
		// Related research links:
		// https://www.reddit.com/r/ProgrammingLanguages/comments/kji3k3/comment/ggx1ftq/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1
		// https://github.com/rust-lang/rust/issues/71519
		// https://github.com/rust-lang/rust/blob/c62239aeb3ba7781a6d7f7055523c1e8c22b409c/compiler/rustc_codegen_ssa/src/back/link.rs#L1442
		system_command::SystemCommand command("gcc");

		for (const auto& object_file_path: inputs) command.addArg(object_file_path.FILE.native());


		if (options.link_c_standard_library) command.addArg("-lc");  // Link the C standard library.

		command.addArg("-o");
		command.addArg(output.FILE.native());
		command.execute();
	}
}
