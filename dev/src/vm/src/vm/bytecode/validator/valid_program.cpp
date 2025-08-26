#include "valid_program.hpp"

#include "errors.hpp"

#include <base/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/function_validator.hpp>
#include <vm/core/process/builtin_functions.hpp>

#include <unordered_set>

vm::code::ValidProgram vm::code::ValidProgram::empty() { return {}; }

vm::code::ValidProgram vm::code::ValidProgram::withBuiltins() {
	auto program         = ValidProgram();
	program.type_context = getBuiltinTypes();
	return program;
}

Box<vm::TypeMetadata> vm::code::ValidProgram::produceTypeMetadata() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return type_context.validateAndProduceTypeMetadata(available_functions);
}

vm::code::ValidProgram vm::code::ValidProgram::newInsertCode(const code::CodeCollection& collection
) const {
	ValidProgram copy = *this;
	copy.insertCode(collection);
	return copy;
}

void vm::code::ValidProgram::insertCode(const CodeCollection& collection) {
	valid = false;

	for (const auto& func: collection.functions) available_functions.put(func.name, func.signature);

	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertFunctions(collection.functions);

	valid = true;
}

void vm::code::ValidProgram::insertTypes(const std::vector<TypeOfData>& new_types) {
	for (const auto& type: new_types) type_context.insertType(type);
}

void vm::code::ValidProgram::insertGlobals(const std::vector<GlobalData>& new_globals) {
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

void vm::code::ValidProgram::insertFunctions(const std::vector<Function>& new_functions) {
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

const vm::ObjIdNameMap<vm::code::TypeOfData>& vm::code::ValidProgram::types() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return type_context.getCurrentTypes();
}

const vm::ObjIdNameMap<vm::code::GlobalData>& vm::code::ValidProgram::globals() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return globals_map;
}

const vm::ObjIdNameMap<vm::code::Function>& vm::code::ValidProgram::functions() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return function_map;
}

vm::code::CodeCollection vm::code::ValidProgram::produceValidCodeCollection() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return { .functions = { function_map.begin(), function_map.end() },
		     .types
		     = { type_context.getCurrentTypes().begin(), type_context.getCurrentTypes().end() },
		     .global_data = { globals_map.begin(), globals_map.end() } };
}
