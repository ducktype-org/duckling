#include "valid_program.hpp"

#include "errors.hpp"

#include <base/string_id.hpp>

#include "vm/bytecode/validator/type_context.hpp"
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/function_validator.hpp>
#include <vm/core/process/builtin_functions.hpp>

vm::code::HighVMProgram vm::code::HighVMProgram::empty() { return {}; }

vm::code::HighVMProgram vm::code::HighVMProgram::withBuiltins() {
	auto program         = HighVMProgram();
	program.type_context = getBuiltinTypes();
	return program;
}

Box<vm::TypeMetadata> vm::code::HighVMProgram::produceTypeMetadata() const {
	return type_context.validateAndProduceTypeMetadata(available_functions);
}

vm::code::HighVMProgram vm::code::HighVMProgram::tryInsertCode(
	const code::CodeCollection& collection
) const {
	HighVMProgram copy = *this;
	copy.insertCode(collection);
	return copy;
}

void vm::code::HighVMProgram::insertCode(const CodeCollection& collection) {
	for (const auto& func: collection.functions) available_functions.put(func.name, func.signature);

	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertFunctions(collection.functions);
}

void vm::code::HighVMProgram::insertTypes(const std::vector<TypeOfData>& new_types) {
	// TODOP: Rebuild types instead of build from zero.
	for (const auto& type: new_types) type_context.insertType(type);
}

void vm::code::HighVMProgram::insertGlobals(const std::vector<GlobalData>& new_globals) {
	for (const auto& global: new_globals) {
		if (globals_map.contains(global.name))
			throw DuplicatedGlobalDataError(global, *globals_map.at(global.name));
		if (global.ctor_name.has_value() && !available_functions.contains(global.ctor_name.value()))
			throw MissingGlobalCtorDtorError(true, global.ctor_name.value(), global.name);
		if (global.dtor_name.has_value() && !available_functions.contains(global.dtor_name.value()))
			throw MissingGlobalCtorDtorError(false, global.dtor_name.value(), global.name);
		globals_map.insert(global, global.name);
	}
}

void vm::code::HighVMProgram::insertFunctions(const std::vector<Function>& new_functions) {
	auto type_metadata = type_context.validateAndProduceTypeMetadata(available_functions);

	for (const auto& func: new_functions) {
		if (function_map.contains(func.name))
			throw DuplicatedFunctionError(func, *function_map.at(func.name));

		auto validated_function = validateAndExtractReachableCode(
			type_context.getCurrentTypes(), *type_metadata, globals_map, available_functions, func
		);
		function_map.insert(validated_function, validated_function.name);
	}
}

const vm::ObjIdNameMap<vm::code::TypeOfData>& vm::code::HighVMProgram::types() const {
	return type_context.getCurrentTypes();
}

const vm::code::TypeContext& vm::code::HighVMProgram::getTypeContext() const {
	return type_context;
}

const vm::ObjIdNameMap<vm::code::GlobalData>& vm::code::HighVMProgram::globals() const {
	return globals_map;
}

const vm::ObjIdNameMap<vm::code::Function>& vm::code::HighVMProgram::functions() const {
	return function_map;
}

vm::code::CodeCollection vm::code::HighVMProgram::produceValidCodeCollection() const {
	return { .functions = { function_map.begin(), function_map.end() },
		     .types
		     = { type_context.getCurrentTypes().begin(), type_context.getCurrentTypes().end() },
		     .global_data = { globals_map.begin(), globals_map.end() } };
}
