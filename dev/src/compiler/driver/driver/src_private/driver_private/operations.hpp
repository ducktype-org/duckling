#pragma once

#include <artifacts/artifacts.hpp>
#include <driver/backend_type.hpp>
#include <helios/hout/hout.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>
#include <lir/lir_structure/lir_structure.hpp>


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



	struct BackendModuleGlobal final {
		lir::LirGlobal lir_global;
		base::Optional<CRef<lir::Function>>
			global_ctor;  // Optional, if the global has a constructor.
		base::Optional<CRef<lir::Function>> global_dtor;  // Optional, if the global has a destructor.
	};

	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backends to generate the final output.
	 */
	struct BackendModuleData final {
		base::StrID                      module_id;
		std::vector<CRef<lir::Function>> functions;
		std::vector<BackendModuleGlobal>
			globals;  ///< Global variables and their constructors/destructors.
	};



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
