#include "program.hpp"
#include <base/ref.hpp>

base::Optional<CRef<vm::FuncData>> vm::LowVMProgram::getFuncByName(base::StrID name) const {
	auto data = func_name_to_id.atMaybeCopy(name);
	if_opt_some(data, func_id) return &functions[func_id];
	return {};
}

base::Optional<CRef<vm::FuncData>> vm::LowVMProgram::getFuncByID(usize id) const {
	return functions.getCRef(id);
}

base::Optional<CRef<vm::Type>> vm::LowVMProgram::getTypeByName(base::StrID name) const {
	return type_metadata->getTypeByName(name);
}

bool vm::LowVMProgram::addFunction(const base::StrID& func_name, const FuncData& func) {
	if (func_name_to_id.contains(func_name)) return false;
	auto func_id = functions.pushBack(func);
	func_name_to_id.put(func_name, func_id);
	return true;
}

CRef<vm::Type> vm::LowVMProgram::getTypeByID(TypeID id) const { return type_metadata->getType(id); }

usize vm::LowVMProgram::getNumberOfFunctions() const { return functions.size(); }
