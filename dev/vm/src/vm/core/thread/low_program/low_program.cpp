#include "low_program.hpp"

#include <base/ref.hpp>

base::Optional<CRef<vm::low::FuncData>> vm::low::LowVMProgram::getFuncByName(base::StrID name
) const {
	auto data = func_name_to_id.atMaybeCopy(name);
	if_opt_some(data, func_id) return functions[func_id];
	return {};
}

base::Optional<CRef<vm::low::FuncData>> vm::low::LowVMProgram::getFuncByID(usize id) const {
	return functions[id];
}

base::Optional<CRef<vm::Type>> vm::low::LowVMProgram::getTypeByName(base::StrID name) const {
	return type_metadata->getTypeByName(name);
}

bool vm::low::LowVMProgram::addFunction(const base::StrID& func_name, const FuncData& func) {
	if (func_name_to_id.contains(func_name)) return false;
	functions.pushBack(func);
	func_name_to_id.put(func_name, functions.lastIndex());
	return true;
}

CRef<vm::Type> vm::low::LowVMProgram::getTypeByID(TypeID id) const {
	return type_metadata->getType(id);
}

usize vm::low::LowVMProgram::getNumberOfFunctions() const { return functions.size(); }
