#pragma once

#include "driver.hpp"

#include <base/box.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::driver {

	class DVMDriver final: public BackendDriver {
		std::vector<vm::code::CodeCollection> code_collection{};

	public:
		DVMDriver(CRef<Options> options): BackendDriver(options) {}

		void compileModule(query::Context&, const BackendModuleData&) override;

		void link(base::StrID output_file) final;

		auto run() -> std::expected<RunOutput, std::string> final;
	};
}
