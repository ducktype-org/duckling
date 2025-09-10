/**
 * Implementation of high-level operations of the compiler.
 * It is possible to use core compiler directly, but all standard high-level operations
 * should be done through this interface.
 */

#pragma once

#include "../backend_type.hpp"

#include <frontend/module_tree/module_id.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::driver {

	/**
	 * Temporary interface for compiling the entire package into a single binary.
	 * It compiler every module into the .o/.dbc files (via queries),
	 * and also for LLVM backend it links them into a single binary.
	 */
	void compilerEntirePackage(const fs::File& package_location, BackendType backend);

	struct RunOutput final {
		int exit_code;
	};

	/**
	 * Temporary interface for compiling and running code on DVM in-memory.
	 */
	std::expected<RunOutput, std::string> runModuleOnDVM(
		query::Context& ctx, frontend::ModuleID module_id
	);

	struct KeyOf_CompileModule final {
		frontend::ModuleID module_id;
		BackendType        backend_type;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * Query that produces .dbc/.o file for given Duckling module.
	 */
	DECLARE_QUERY(CompileModule, KeyOf_CompileModule, artifacts::FileArtifact);
}
