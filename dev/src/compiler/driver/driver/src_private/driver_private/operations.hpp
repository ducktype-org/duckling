#pragma once

#include <artifacts/artifacts.hpp>
#include <driver/backend_type.hpp>
#include <helios/hout/hout.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>
#include "backend_module_data.hpp"


namespace compiler::driver {

	/**
	 * @brief Compiles the HOUTUnit to the backend module.
	 * Only used internally by the driver.
	 */
	void compileHOUTUnit(
		query::Context&                        ctx,
		base::CRef<compiler::helios::HOUTUnit> hout_unit,
		base::StrID                            module_id,
		artifacts::FileArtifact                output_artifact,
		BackendType                            backend_type
	);



	/**
	* @brief Compile given the LIR functions and module data to the backend module.
	* Outputs the module value.
	* @param module_data
	*/
	void compileBackendModule(
		query::Context&          ctx,
		const BackendModuleData& module_data,
		artifacts::FileArtifact  output_artifact,
		BackendType              backend_type
	);


	// todo PR: move elsewhere
	/**
	 * Compile builtin LLVM library into an object file.
	 */
	artifacts::FileArtifact emitBuiltinLLVMObjectFile();
}
