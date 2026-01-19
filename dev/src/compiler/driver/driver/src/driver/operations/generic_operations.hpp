/**
 * Implementation of high-level operations of the compiler.
 * It is possible to use core compiler directly, but all standard high-level operations
 * should be done through this interface.
 */

#pragma once

#include "../backend_type.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::driver {

	/**
	 * Temporary interface for compiling the entire package into a single binary.
	 * It compiler every module into the .o/.dbc files (via queries),
	 * and also for LLVM backend it links them into a single binary.
	 *
	 * @brief The final link step for creating the package executable.
	 * \parallel Must be serialized or guarded to avoid overwriting/colliding outputs when packaging
	 * concurrently.
	 */
	base::OkBad compileEntirePackage(
		const global_state::PackageInfo& package_info,
		BackendType                      backend,
		const linker::LinkingOptions&    linking_options
	);

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

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const;
	};

	/**
	 * Query that produces .dbc/.o file for given Duckling module.
	 *
	 * \parallel
	 * - Writes artifacts (\ref artifact::ArtifactCollection)
	 * - Produces processed files (artifact outputs, LLVM/DVM intermediates)
	 * - Updates backend compilation timer (\ref timer::AddToTime)
	 * - Uses \ref compiler::frontend::ModuleTree::getPathComponentHash (lazy \ref
	 * compiler::frontend::ModuleTree mutation)
	 * \query_not_thread_safe
	 */
	DECLARE_QUERY(
		CompileModule,
		KeyOf_CompileModule,
		query::QResult<artifacts::FileArtifact>,
		({ .used_hashes = query::UsedHashes::StableHash, .can_be_loaded_from_disk = true })
	);
}
