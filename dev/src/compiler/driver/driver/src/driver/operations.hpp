/**
 * Implementation of high-level operations of the compiler.
 */

#pragma once

#include <query_framework/query_int.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <artifacts/artifacts.hpp>
#include "backend_type.hpp"

namespace driver {

    /**
     * Temporary interface for compiling the entire main package.
     */
    void compilerEntireMainPackageIntoBinary();


    struct KeyOf_CompileModule final {
		compiler::frontend::ModuleID module_id;
		BackendType        backend_type;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * Query that produces QBC/.o file for given Duckling module.
	 */
	DECLARE_QUERY(CompileModule, KeyOf_CompileModule, artifacts::FileArtifact);


}
