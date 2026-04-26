/**
 * Implementation of high-level operations of the compiler.
 * It is possible to use core compiler directly, but all standard high-level operations
 * should be done through this interface.
 */

#pragma once

#include "../backend_type.hpp"

#include <archiver/archive.hpp>
#include <debug_info/debug_info.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <string>

namespace compiler::driver {

	/**
	 * Temporary interface for compiling the entire package into a single binary.
	 * It compiler every module into the .o/.dbc files (via queries),
	 * and for LLVM backend it links or archives them into a final artifact.
	 *
	 * @brief The final link/archive step for producing the package artifact.
	 * \parallel Must be serialized or guarded to avoid overwriting/colliding outputs when packaging
	 * concurrently.
	 */
	base::OkBad compileEntirePackage(
		const global_state::PackageInfo& package_info, BuildTarget build_target
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

	/**
	 * @brief Compile a Duckling script (.ds file) into a single artifact.
	 *
	 * Reads the script source from global_state::ScriptContext, splits it into individual
	 * statements, creates a chain of REPL-style modules (each with a parent link to the previous),
	 * compiles each one, and combines the results into a single output file:
	 *   - DVM backend  -> .dbc bytecode file
	 *   - LLVM backend -> native executable (linked with linking_options)
	 *
	 * The script file and artifact root must be set in global_state via init before calling this
	 * function.
	 *
	 * @param backend_type     Whether to use DVM or LLVM backend.
	 * @param linking_options  Linker configuration (ignored for DVM backend).
	 */
	base::OkBad compileScript(
		BackendType backend_type, const linker::LinkingOptions& linking_options
	);

	/**
	 * Compile a Duckling script to DVM bytecode in-memory and execute it.
	 * The script file must be set in global_state via init before calling this function.
	 */
	std::expected<RunOutput, std::string> runScriptOnDVM();

	struct KeyOf_CompileModule final {
		frontend::ModuleID module_id;
		BackendType        backend_type;
		bool               build_debug_info;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const;
	};

	struct CompileModuleResult {
		artifacts::FileArtifact               object_art;
		base::Optional<debug_info::DebugInfo> debug_info;
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
		CRef<query::QResult<CompileModuleResult>>,
		({
			.used_hashes             = query::UsedHashes::StableHash,
			.can_be_loaded_from_disk = true,
			.preserve_in_graph       = true,

			// We expect compile module query to not fail with query failed exception during its
	        // execution.
			.catch_exceptions_if_using_qresult = false,
		})
	);
}
