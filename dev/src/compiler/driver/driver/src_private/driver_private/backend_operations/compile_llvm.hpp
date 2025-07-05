#pragma once

#include <query_framework/context_fd.hpp>
#include <artifacts/artifacts.hpp>

namespace compiler::driver {

	void compileBackendModuleToLLVM(
		query::Context&          ctx,
		const BackendModuleData& lir_module,
		artifacts::FileArtifact  output_artifact
	);
}
