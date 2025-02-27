#include "program.hpp"


base::Optional<CRef<vm::FuncData>> vm::VMProgram::getFuncByName(base::StrID name) const {
	auto data = func_name_to_id.atMaybeCopy(name);
	if_opt_some(data, func_id) return &functions[func_id];
	return {};
}

base::Optional<CRef<vm::FuncData>> vm::VMProgram::getFuncByID(usize id) const {
	return functions.getCRef(id).expect("Bad FuncID in getFuncByID");
}

base::Optional<CRef<vm::Type>> vm::VMProgram::getTypeByName(base::StrID name) const {
	return type_metadata.getTypeByName(name);
}

bool vm::VMProgram::addFunction(const base::StrID& func_name, const FuncData& func) {
	if (func_name_to_id.contains(func_name)) {
		return false;
	}
	auto func_id = functions.pushBack(std::move(func));
	func_name_to_id.put(std::move(func_name), std::move(func_id));
	return true;
}

CRef<vm::Type> vm::VMProgram::getTypeByID(TypeID id) const { return type_metadata.getType(id); }

usize vm::VMProgram::getNumberOfFunctions() const {
	return functions.size();
}