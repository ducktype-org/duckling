#pragma once

#include <base/string_id.hpp>
#include <query_framework/context_fd.hpp>
 
#include "driver.hpp"

namespace compiler::driver {
	class LLVMDriver final: public BackendDriver {
		std::vector<base::StrID> object_file_paths;

	public:
		LLVMDriver(CRef<Options> options): BackendDriver(options) {}

		void compileModule(query::Context& ctx, const BackendModuleData& lir_module) final;

		void link() final;
	};

}
