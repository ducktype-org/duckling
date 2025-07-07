#pragma once

#include <query_framework/context_fd.hpp>
#include <artifacts/artifacts.hpp>
#include "../backend_module_data.hpp"

namespace compiler::driver {

	void compileBackendModuleToLLVM(
		query::Context&          ctx,
		const BackendModuleData& lir_module,
		const artifacts::FileArtifact&  output_artifact
	);
}
