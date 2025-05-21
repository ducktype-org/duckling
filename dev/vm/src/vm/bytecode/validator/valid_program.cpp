#include "valid_program.hpp"

#include "errors.hpp"

#include "vm/bytecode/bytecode.hpp"
#include "vm/bytecode/validator/function_validator.hpp"
#include <vm/bytecode/builtin_types.hpp>

vm::code::ValidProgram::ValidProgram(): current_type_metadata(makeBox<TypeMetadata>()) {}

vm::code::ValidProgram vm::code::ValidProgram::empty() { return {}; }

vm::code::ValidProgram vm::code::ValidProgram::withBuiltins() {
	auto program         = ValidProgram();
	program.type_context = getBuiltinTypes();
	return program;
}

vm::code::CodeCollection vm::code::ValidProgram::produceValidBytecode() const {
	CodeCollection code;
	for (const auto& type: type_context_builder.getTypes()) code.types.push_back(type);
	for (const auto& global: globals_map) code.global_data.push_back(global);
	for (const auto& function: functions) code.functions.push_back(function);
	return code;
}

Box<vm::TypeMetadata> vm::code::ValidProgram::produceTypeMetadata() const {
	return type_context.build().moveMetadata();
}

vm::code::ValidProgram vm::code::ValidProgram::withCode(
	const std::vector<code::CodeCollection>& collections_to_add
) const {
	ValidProgram copy = *this;
	for (const auto& collection: collections_to_add) {
		copy.insertTypes(collection.types);
		copy.insertGlobals(collection.global_data);
		copy.insertFunctions(collection.functions);
	}
	return copy;
}

void vm::code::ValidProgram::insertTypes(const std::vector<code::TypeOfData>& new_types) {
	for (const auto& type: new_types) type_context.insertType(type);
}

void vm::code::ValidProgram::insertGlobals(const std::vector<code::GlobalData>& new_globals) {
    // @TODO: Validate global's type even exists.
	for (const auto& global: new_globals) globals_map.insert(global, global.name);
}

void vm::code::ValidProgram::insertFunctions(const std::vector<vm::code::Function>& new_functions) {
    auto type_metadata = type_context.validateAndProduceTypeMetadata();
	for (const auto& func: new_functions) {
        validateAndExtractReachableCode(type_context.getCurrentTypes(), , const StableTypeIdNameMap<GlobalData> &globals_map, const Function &function)
	}
}
