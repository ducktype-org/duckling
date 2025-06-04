#pragma once

#include "backend_driver.hpp"

#include <base/box.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::driver {

	class DVMDriver final: public BackendDriver {
		std::vector<vm::code::CodeCollection> code_collection{};

	public:
		DVMDriver(CRef<BackendOptions> options): BackendDriver(options) {}

		void compileModule(query::Context&, const BackendModuleData&, artifacts::FileArtifact output_artifact) override;

		auto run() -> std::expected<RunOutput, std::string> final;
	};
}
