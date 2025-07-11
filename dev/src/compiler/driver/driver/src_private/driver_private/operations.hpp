#pragma once

#include "backend_module_data.hpp"

#include <artifacts/artifacts.hpp>
#include <driver/backend_type.hpp>
#include <helios/hout/hout.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>

namespace compiler::driver {

	// /**
	//  * @brief Compiles the HOUTUnit to the backend module.
	//  * Only used internally by the driver.
	//  * @param module_data MModule to compile.
	//  * @param output_artifact Artifact to write the compiled module to.
	//  */
	// void compileHOUTUnit(
	// 	query::Context&                        ctx,
	// 	base::CRef<compiler::helios::HOUTUnit> hout_unit,
	// 	base::StrID                            module_id,
	// 	const artifacts::FileArtifact&         output_artifact,
	// 	BackendType                            backend_type
	// );

	BackendModuleData compileHOUTUnitToBackendModuleData(
		query::Context& ctx, base::CRef<compiler::helios::HOUTUnit> hout_unit, base::StrID module_id
	);


	// /**
	//  * @brief Compile given the LIR functions and module data to the backend module.
	//  * Outputs the module value.
	//  * @param module_data MModule to compile.
	//  * @param output_artifact Artifact to write the compiled module to.
	//  */
	// void compileBackendModule(
	// 	query::Context&                ctx,
	// 	const BackendModuleData&       module_data,
	// 	const artifacts::FileArtifact& output_artifact,
	// 	BackendType                    backend_type
	// );


}
