#include "low_program.hpp"
#include <base/ref.hpp>

base::Optional<CRef<vm::low::FuncData>> vm::low::LowVMProgram::funcAtMaybe(base::StrID name) const {
	return functions.atMaybe(name);
}

base::Optional<CRef<vm::low::FuncData>> vm::low::LowVMProgram::funcAtMaybe(usize id) const {
	return functions.atMaybe(id);
}

base::Optional<CRef<vm::Type>> vm::low::LowVMProgram::typeAtMaybe(base::StrID name) const {
	return types.atMaybe(name);
}

CRef<vm::Type> vm::low::LowVMProgram::typeAt(TypeID id) const { return types.at(id); }

usize vm::low::LowVMProgram::getNumberOfFunctions() const { return functions.size(); }

base::Optional<CRef<vm::Type>> vm::low::LowVMProgram::typeAtMaybe(TypeID id) const {
	return types.atMaybe(id);
}

bool vm::low::LowVMProgram::addFunction(const FuncData& func) {
	if (functions.contains(func.name)) return false;
	functions.insert(func, func.name);
	return true;
}
