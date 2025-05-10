#pragma once

#include "driver.hpp"

#include <query_framework/context_fd.hpp>

#include <base/string_id.hpp>

namespace compiler::driver {
	class LLVMDriver final: public BackendDriver {
		std::vector<base::StrID> object_file_paths;

	public:
		LLVMDriver(CRef<Options> options): BackendDriver(options) {}

		void compileModule(query::Context& ctx, const BackendModuleData& lir_module) final;

		void link() final;

		void run() final;
	};

	inline void LLVMDriver::run() {};
}
