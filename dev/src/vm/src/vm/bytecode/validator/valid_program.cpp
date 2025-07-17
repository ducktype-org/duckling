#include "valid_program.hpp"
#include <iostream>
#include "base/maps.hpp"

#include "errors.hpp"

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/function_validator.hpp>

vm::code::ValidProgram vm::code::ValidProgram::empty() { return {}; }

vm::code::ValidProgram vm::code::ValidProgram::withBuiltins() {
	auto program         = ValidProgram();
	program.type_context = getBuiltinTypes();
	return program;
}

Box<vm::TypeMetadata> vm::code::ValidProgram::produceTypeMetadata() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return type_context.validateAndProduceTypeMetadata();
}

vm::code::ValidProgram vm::code::ValidProgram::newInsertCode(const code::CodeCollection& collection
) const {
	ValidProgram copy = *this;
	copy.insertCode(collection);
	return copy;
}

void vm::code::ValidProgram::insertCode(const code::CodeCollection& collection) {
	valid = false;
	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertFunctions(collection.functions);
	valid = true;
}

void vm::code::ValidProgram::insertTypes(const std::vector<code::TypeOfData>& new_types) {
	for (const auto& type: new_types) type_context.insertType(type);
}

void vm::code::ValidProgram::insertGlobals(const std::vector<code::GlobalData>& new_globals) {
	for (const auto& global: new_globals) {
		if (globals_map.contains(global.name))
			throw DuplicatedGlobalDataError(global, *globals_map.at(global.name));
		globals_map.insert(global, global.name);
	}
}

void vm::code::ValidProgram::insertFunctions(const std::vector<vm::code::Function>& new_functions) {
	auto type_metadata = type_context.validateAndProduceTypeMetadata();

	base::HashMap<base::StrID, vm::code::Signature> signatures;

	for (const auto& func: new_functions) {
		Signature signature;
		signature.result_type = func.result_type.str;
		signature.parameters.reserve(func.parameters.size());
		for (const auto& param: func.parameters) {
			signature.parameters.emplace_back(param.str);
		}
		signatures.put(func.name.str, signature);
	}

	for (const auto& [func_ref, id, name] : function_map.allData()) {
		Signature signature;
		signature.result_type = func_ref->result_type.str;
		signature.parameters.reserve(func_ref->parameters.size());
		for (const auto& param: func_ref->parameters) {
			signature.parameters.emplace_back(param.str);
		}
		signatures.put(name, signature);
	}

	for (const auto& type: type_context.getCurrentTypes()) {
		if (std::holds_alternative<vm::code::FunctionType>(type)) {
			const auto& function_type = std::get<vm::code::FunctionType>(type);
			Signature signature;
			signature.result_type = function_type.result;
			signature.parameters.reserve(function_type.parameters.size());
			for (const auto& param: function_type.parameters) {
				signature.parameters.emplace_back(param);
			}
			signatures.put(function_type.name, signature);
		}
	}

	for (const auto& func: new_functions) {
		if (function_map.contains(func.name))
			throw DuplicatedFunctionError(func, *function_map.at(func.name));

		auto validated_function = validateAndExtractReachableCode(
			type_context.getCurrentTypes(), signatures, *type_metadata, globals_map, func
		);
		function_map.insert(validated_function, validated_function.name);
		std::vector<base::StrID> parameters;
		for (const auto& param: validated_function.parameters) {
			parameters.emplace_back(param.str);
		}
		FunctionType function_type(
			validated_function.name.str,
			parameters,
			validated_function.result_type.str
		);
		type_context.insertType(function_type);
	}
}

const vm::StableObjIdNameMap<vm::code::TypeOfData>& vm::code::ValidProgram::types() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return type_context.getCurrentTypes();
}

const vm::StableObjIdNameMap<vm::code::GlobalData>& vm::code::ValidProgram::globals() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return globals_map;
}

const vm::StableObjIdNameMap<vm::code::Function>& vm::code::ValidProgram::functions() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return function_map;
}

vm::code::CodeCollection vm::code::ValidProgram::produceValidCodeCollection() const {
	CORE_ASSERT(valid, "Using an invalidated ValidProgram");
	return {
		.functions = { function_map.begin(), function_map.end() },
		.types = { type_context.getCurrentTypes().begin(), type_context.getCurrentTypes().end() },
		.global_data = { globals_map.begin(), globals_map.end() },
	};
}
