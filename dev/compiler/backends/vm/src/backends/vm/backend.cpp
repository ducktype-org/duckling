#include "backend.hpp"
#include "instructions.hpp"

namespace compiler::backend_vm {

	void Module::buildRepr(std::ostream& out) const { file_builder.build().serialize(out); }

	void Module::addLirFunction(CRef<lir::Function> lir_function) {}

	Module::Module(base::StrID module_id): module_id(module_id) {}
}
