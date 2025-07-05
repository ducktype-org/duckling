/**
 * Implementation of high-level operations of the compiler.
 */

#pragma once

#include "backend_type.hpp"

#include <artifacts/artifacts.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::driver {

	/**
	 * Temporary interface for compiling the entire main package.
	 */
	void compilerEntireMainPackageIntoBinary(
		const fs::FilePath& package_location,
		BackendType backend
	);

	struct KeyOf_CompileModule final {
		frontend::ModuleID module_id;
		BackendType        backend_type;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * Query that produces QBC/.o file for given Duckling module.
	 */
	DECLARE_QUERY(CompileModule, KeyOf_CompileModule, artifacts::FileArtifact);


}
