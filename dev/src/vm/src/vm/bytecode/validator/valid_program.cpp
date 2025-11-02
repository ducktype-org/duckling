#include "valid_program.hpp"

#include "errors.hpp"

#include <base/str/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/function_validator.hpp>
#include <vm/bytecode/validator/type_builder.hpp>
#include <vm/bytecode/validator/type_validator.hpp>

vm::code::ValidProgram vm::code::ValidProgram::empty() { return {}; }

vm::code::ValidProgram vm::code::ValidProgram::withBuiltins() {
	auto program         = ValidProgram();
	program.type_context = getBuiltinTypes();
	return program;
}

vm::code::CodeCollection vm::code::ValidProgram::produceValidCodeCollection() const {
	return { .functions            = std::ranges::to<std::vector>(function_map),
		     .types                = std::ranges::to<std::vector>(type_context.getCurrentTypes()),
		     .global_data          = std::ranges::to<std::vector>(globals_map),
		     .external_c_functions = std::ranges::to<std::vector>(ext_c_function_map) };
}

vm::code::ValidProgram vm::code::ValidProgram::tryInsertCode(const code::CodeCollection& collection
) const {
	// @TODO: #1306 We could get rid of copying of the whole program.
	ValidProgram copy = *this;
	copy.insertCode(collection);
	return copy;
}

const vm::ObjIdNameMap<vm::code::TypeOfData>& vm::code::ValidProgram::types() const {
	return type_context.getCurrentTypes();
}

const vm::code::TypeContext& vm::code::ValidProgram::getTypeContext() const { return type_context; }

const vm::ObjIdNameMap<vm::code::GlobalData>& vm::code::ValidProgram::globals() const {
	return globals_map;
}

const vm::ObjIdNameMap<vm::code::Function>& vm::code::ValidProgram::functions() const {
	return function_map;
}

void vm::code::ValidProgram::insertCode(const CodeCollection& collection) {
	for (const auto& func: collection.functions) available_functions.put(func.name, func.signature);
	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertExternalCFunctions(collection.external_c_functions);
	insertFunctions(collection.functions);
}

void vm::code::ValidProgram::insertTypes(const std::vector<TypeOfData>& new_types) {
	for (const auto& type: new_types) type_context.insertType(type);
	// Check if no cycles in hierarchy appeared after injection.
	detail::validateTypesIntegrity(type_context);
	// Validate only the newly added types.
	for (const auto& type: new_types) detail::validateType(type, type_context, available_functions);
}

void vm::code::ValidProgram::insertGlobals(const std::vector<GlobalData>& new_globals) {
	for (const auto& global: new_globals) {
		if (globals_map.contains(global.name))
			throw DuplicatedGlobalDataError(global, *globals_map.at(global.name));
		if (!type_context.getCurrentTypes().contains(global.type))
			throw UnknownTypeError(opargs::Type(global.type));
		if (global.ctor_name.has_value() && !available_functions.contains(global.ctor_name.value()))
			throw MissingGlobalCtorDtorError(true, global.ctor_name.value(), global.name);
		if (global.dtor_name.has_value() && !available_functions.contains(global.dtor_name.value()))
			throw MissingGlobalCtorDtorError(false, global.dtor_name.value(), global.name);
		globals_map.insert(global, global.name);
	}
}

void vm::code::ValidProgram::insertFunctions(const std::vector<Function>& new_functions) {
	if (new_functions.empty()) return;

	// @note: This is a temporary built type metadata for the sake of function verification.
	// @TODO: #1306
	auto type_metadata = detail::buildTypeMetadata(type_context);

	for (const auto& func: new_functions) {
		if (function_map.contains(func.name))
			throw DuplicatedFunctionError(func, *function_map.at(func.name));

		auto validated_function = detail::validateAndExtractReachableCode(
			type_context.getCurrentTypes(),
			*type_metadata,
			globals_map,
			available_functions,
			ext_c_function_map,
			func
		);
		function_map.insert(validated_function, validated_function.name);
	}
}

void vm::code::ValidProgram::insertExternalCFunctions(
	const std::vector<ExternalCFunction>& new_functions
) {
	if (new_functions.empty()) return;

	// @TODO: #1306 Fix
	auto type_metadata = detail::buildTypeMetadata(type_context);

	for (const auto& new_func: new_functions) {
		if (ext_c_function_map.contains(new_func.name))
			throw DuplicatedExtCFunctionError(new_func, *ext_c_function_map.at(new_func.name));

		// Validate arguments exist and are trivially copyable
#ifdef __cpp_lib_ranges_concat
		for (const auto& type: std::views::concat(
				 new_func.signature.parameters, std::views::single(new_func.signature.result_type)
			 ))
			if (auto tp = type_metadata->atMaybe(type)) {
				if (!tp.value()->isTriviallyCopyable())
					throw ExtCArgumentTypeNotTriviallyCopyable(*tp);
			} else
				throw UnknownTypeError(opargs::Type(type));
#else
		if (auto tp = type_metadata->atMaybe(new_func.signature.result_type)) {
			if (!tp.value()->isTriviallyCopyable()) throw ExtCArgumentTypeNotTriviallyCopyable(*tp);
		} else
			throw UnknownTypeError(opargs::Type(new_func.signature.result_type));
		for (const auto& type: new_func.signature.parameters)
			if (auto tp = type_metadata->atMaybe(type)) {
				if (!tp.value()->isTriviallyCopyable())
					throw ExtCArgumentTypeNotTriviallyCopyable(*tp);
			} else
				throw UnknownTypeError(opargs::Type(type));
#endif

		ext_c_function_map.insert(new_func, new_func.name);
	}
}

const vm::ObjIdNameMap<vm::code::ExternalCFunction>& vm::code::ValidProgram::extCFunctions() const {
	return ext_c_function_map;
}
