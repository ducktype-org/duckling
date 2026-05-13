#include "valid_program.hpp"

#include "errors.hpp"

#include <string_id/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/function_validator.hpp>
#include <vm/bytecode/validator/type_validator.hpp>

vm::code::ValidProgram vm::code::ValidProgram::withBuiltins() {
	auto program = ValidProgram();
	program.insertTypes(getBuiltinTypes());
	return program;
}

vm::code::CodeCollection vm::code::ValidProgram::produceValidCodeCollection() const {
	return { .functions            = std::ranges::to<std::vector>(function_map),
		     .types                = std::ranges::to<std::vector>(type_context.getTodTypes()),
		     .global_data          = std::ranges::to<std::vector>(globals_map),
		     .external_c_functions = std::ranges::to<std::vector>(ext_c_function_map) };
}

vm::code::ValidProgram vm::code::ValidProgram::tryInsertCode(
	const code::CodeCollection& collection, api::ExecutionConfig config
) const {
	// @TODO: #1306 We could get rid of copying of the whole program.
	ValidProgram copy = *this;
	copy.insertCode(collection, config);
	return copy;
}

const vm::code::valid_type::ValidTypeMap& vm::code::ValidProgram::types() const {
	return type_context.getCurrentTypes();
}

const vm::code::TypeContext& vm::code::ValidProgram::getTypeContext() const { return type_context; }

const vm::ObjIdNameMap<vm::code::GlobalData>& vm::code::ValidProgram::globals() const {
	return globals_map;
}

const vm::ObjIdNameMap<vm::code::Function>& vm::code::ValidProgram::functions() const {
	return function_map;
}

void vm::code::ValidProgram::insertCode(
	const CodeCollection& collection, api::ExecutionConfig config
) {
	for (const auto& func: collection.functions) function_signatures.put(func.name, func.signature);
	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertExternalCFunctions(collection.external_c_functions);
	insertFunctions(collection.functions, config);
}

void vm::code::ValidProgram::insertTypes(const std::vector<TypeOfData>& new_types) {
	type_context.insertAndValidate(new_types, function_signatures);
}

void vm::code::ValidProgram::insertGlobals(const std::vector<GlobalData>& new_globals) {
	for (const auto& global: new_globals) {
		if (globals_map.contains(global.name))
			throw DuplicatedGlobalDataError(global, *globals_map.at(global.name));
		if (!type_context.getCurrentTypes().contains(global.type))
			throw UnknownTypeError(opargs::Type(global.type));
		if (global.ctor_name.has_value() && !function_signatures.contains(global.ctor_name.value()))
			throw MissingGlobalCtorDtorError(true, global.ctor_name.value(), global.name);
		if (global.dtor_name.has_value() && !function_signatures.contains(global.dtor_name.value()))
			throw MissingGlobalCtorDtorError(false, global.dtor_name.value(), global.name);
		globals_map.insert(global, global.name);
	}
}

void vm::code::ValidProgram::insertFunctions(
	const std::vector<Function>& new_functions, api::ExecutionConfig config
) {
	if (new_functions.empty()) return;

	for (const auto& func: new_functions) {
		if (function_map.contains(func.name))
			throw DuplicatedFunctionError(func, *function_map.at(func.name));

		auto validated_function = detail::validateAndExtractReachableCode(
			type_context.getCurrentTypes(), globals_map, function_signatures, ext_c_function_map, func
		);
		function_map.insert(validated_function, validated_function.name);
	}

	flag_context.insertAndValidate(new_functions, globals_map, ext_c_function_map, config);
}

void vm::code::ValidProgram::insertExternalCFunctions(
	const std::vector<ExternalCFunction>& new_functions
) {
	if (new_functions.empty()) return;

	for (const auto& new_func: new_functions) {
		if (ext_c_function_map.contains(new_func.name))
			throw DuplicatedExtCFunctionError(new_func, *ext_c_function_map.at(new_func.name));

		// Validate arguments exist and are trivially copyable

		if (new_func.signature.result_types.size()) {
			CORE_ASSERT(
				new_func.signature.result_types.size() == 1, "C functions return only one type"
			);
			if (auto tp
			    = type_context.getCurrentTypes().atMaybe(new_func.signature.result_types[0])) {
				if (!tp.value()->isTriviallyCopyable())
					throw ExtCArgumentTypeNotTriviallyCopyable(*tp.value());
			} else
				throw UnknownTypeError(opargs::Type(new_func.signature.result_types[0]));
		}

		for (const auto& type: new_func.signature.parameters)
			if (auto tp = type_context.getCurrentTypes().atMaybe(type)) {
				if (!tp.value()->isTriviallyCopyable())
					throw ExtCArgumentTypeNotTriviallyCopyable(*tp.value());
			} else
				throw UnknownTypeError(opargs::Type(type));

		ext_c_function_map.insert(new_func, new_func.name);
	}
}

const vm::ObjIdNameMap<vm::code::ExternalCFunction>& vm::code::ValidProgram::extCFunctions() const {
	return ext_c_function_map;
}
