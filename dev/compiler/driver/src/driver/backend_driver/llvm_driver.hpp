#pragma once

#include "backend_driver.hpp"

#include <query_framework/context_fd.hpp>

#include <base/string_id.hpp>

namespace compiler::driver {
	class LLVMDriver final: public BackendDriver {
		std::vector<base::StrID> object_file_paths;

	public:
		LLVMDriver(CRef<BackendOptions> options): BackendDriver(options) {}

		void compileModule(query::Context& ctx, const BackendModuleData& lir_module) final;

		void link(base::StrID output_file) final;

		std::expected<RunOutput, std::string> run() final;
	};

	inline std::expected<RunOutput, std::string> LLVMDriver::run() {
		CORE_PANIC("LLVM backend doesn't support run.");
		return std::unexpected<std::string>{ "Run command is not supported in the LLVM Backend." };
	}
}
