#include "driver.hpp"

#include "dvm_driver.hpp"
#include "llvm_driver.hpp"

#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>

namespace compiler::driver {

	Box<BackendDriver> createBackendDriver(CRef<Options> options) {
		switch (options->backend_type) {
		case BackendType::LLVM:
			return base::makeBox<LLVMDriver>(options);
		case BackendType::DVM:
			return base::makeBox<DVMDriver>(options);
		default:
			CORE_PANIC("Wrong enum value");
		}
	}

	void Driver::compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id) {
		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			CRef mir_function
				= &query::entryPoint<mir::LowerToMirFunction>({ hout_function })->value();
			auto lir_function = query::entryPoint<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		BackendModuleData module_data{ .module_id = module_id, .functions = functions };

		query::utils::withContextDo([&](query::Context& ctx) {
			backend_driver->compileModule(ctx, module_data);
		});
	}

	void Driver::link() { backend_driver->link(); }

	std::expected<RunOutput, std::string> Driver::run() { return backend_driver->run();}
}
