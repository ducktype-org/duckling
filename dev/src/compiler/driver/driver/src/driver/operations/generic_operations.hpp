/**
 * Implementation of high-level operations of the compiler.
 * It is possible to use core compiler directly, but all standard high-level operations
 * should be done through this interface.
 */

#pragma once

#include <archiver/archive.hpp>
#include <debug_info/debug_info.hpp>
#include <driver/backend_type.hpp>
#include <driver/task/task.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <string>
#include <vector>

namespace compiler::driver {

	/**
	 * Interface for compiling the packages defined in global_state::packages.
	 * Each package is compiled according to its compilation strategy defined in the
	 * PackageCompilationTask:
	 * - DVM strategy: compiles each module into .dbc files, no linking step.
	 * - Native strategy: compiles each module into .o files, then links them into a final
	 * executable using the specified linking options.
	 * - Lib strategy: compiles each module into .o files, then archives them into a final static
	 * library.
	 * - LLVM strategy: compiles each module into .o files only
	 *
	 * @brief The final link/archive step for producing the package artifact.
	 * \parallel Must be serialized or guarded to avoid overwriting/colliding outputs when packaging
	 * concurrently.
	 */
	base::OkBad compilePackages(const std::vector<PackageCompilationTask>& tasks);

	/**
	 * @brief Compile a single package.
	 * In normal compilation mode, compilePackages should be used
	 * This function is only for testing purpose
	 */
	inline base::OkBad compileEntirePackage(
		const compiler::frontend::packages::PackageInfo& package_info,
		const BuildTarget&                               build_target
	) {
		return compilePackages({
			PackageCompilationTask{
				.root_module  = package_info.getRootModule().illegalAccess().getID(),
				.build_target = build_target,
			},
		});
	}

	struct RunOutput final {
		int exit_code;
	};

	/**
	 * @brief Compile a Duckling script (.dks file) into a single artifact.
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
	 * @param backend_type  Whether to use DVM or LLVM backend.
	 * @param std_lib_opts  Standard library configuration.
	 * @param linking_opts  Linking configuration.
	 */
	base::OkBad compileScript(
		BackendType                          backend_type,
		const options_types::StdLibOptions&  std_lib_opts,
		const options_types::LinkingOptions& linking_opts
	);

	/**
	 * Compile a Duckling script to DVM bytecode in-memory and execute it.
	 * The script file must be set in global_state via init before calling this function.
	 */
	std::expected<RunOutput, std::string> runScriptOnDVM(bool load_stdlib);

	struct KeyOf_CompileModule final {
		frontend::ModuleID module_id;
		BackendType        backend_type;
		bool               build_debug_info;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const;
	};

	struct CompileModuleResult final {
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
