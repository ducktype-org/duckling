#include "dvm_driver.hpp"
#include <fstream>

#include <backends/dvm/backend.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

namespace compiler::driver {
	void DVMDriver::compileModule(query::Context& ctx, const BackendModuleData& data) {
		backend_vm::Module module{
			data.module_id,
			data.functions
		};
		code_collection = module.build();
		std::ofstream dvm_file(
			base::strConcat(data.module_id.strView(), ".dbc").c_str(),
			std::ios::binary
		);
		if (!dvm_file.is_open()) {
			CORE_PANIC("Failed to open DVM file for writing");
		}
		vm::code::serialize(code_collection, dvm_file);
		dvm_file.close();
	}

	void DVMDriver::link() {
		// DVM doesn't require linking.
	}

	void DVMDriver::run() {

    }
}
