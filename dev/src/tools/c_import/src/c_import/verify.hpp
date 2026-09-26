#pragma once

#include <filesystem>
#include <string>

namespace c_import {

	struct VerifyResult final {
		bool ok = false;
		/** The command that was run, so a failure can be reproduced by hand. */
		std::string command;
		/** Whatever the compiler wrote, trimmed to something readable. */
		std::string output;
	};

	/**
	 * @brief Compiles the generated package to check that it is actually valid Duckling.
	 *
	 * Built as a static library, because a bindings package has no `main` to link. Anything
	 * the translator got wrong - a type it should have skipped, a name it should not have used -
	 * surfaces here rather than in the user's build.
	 */
	VerifyResult verifyPackage(
		const std::filesystem::path& out_dir,
		const std::string&           package_name,
		const std::string&           duckc
	);

}
