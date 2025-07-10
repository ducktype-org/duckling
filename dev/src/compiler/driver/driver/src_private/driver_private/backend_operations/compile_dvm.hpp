#pragma once

#include "../backend_module_data.hpp"

#include <artifacts/artifacts.hpp>
#include <query_framework/context_fd.hpp>

#include <base/box.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::driver {

	// TODO: this PR
	// class DVMDriver final {
	// 	std::vector<vm::code::CodeCollection> code_collection{};

	// public:
	// 	//HMMM:
	// 	auto run() -> std::expected<RunOutput, std::string> final;
	// };

	void compileBackendModuleToDVM(
		query::Context&                ctx,
		const BackendModuleData&       lir_module,
		const artifacts::FileArtifact& output_artifact
	);
}
