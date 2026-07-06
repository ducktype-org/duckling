#include "valid_program.hpp"

#include "errors.hpp"

#include <string_id/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/ffi_type_builder.hpp>
#include <vm/bytecode/validator/function_validator.hpp>
#include <vm/bytecode/validator/initial_value.hpp>
#include <vm/bytecode/validator/type_validator.hpp>

vm::code::ValidProgram vm::code::ValidProgram::withBuiltins() {
	auto program = ValidProgram();
	program.insertTypes(getBuiltinTypes());
	return program;
}

vm::code::CodeCollection vm::code::ValidProgram::produceValidCodeCollection() const {
	return { .functions = function_map | std::views::transform([](const auto& valid_function) {
							  return valid_function.toNormal();
						  })
		                | std::ranges::to<std::vector>(),
		     .types                = std::ranges::to<std::vector>(type_context.getTodTypes()),
		     .global_data          = std::ranges::to<std::vector>(globals_map),
		     .external_c_functions = std::ranges::to<std::vector>(ext_c_function_map),
		     .ffi_functions        = std::ranges::to<std::vector>(ffi_function_map),
		     .object_files = std::ranges::to<std::vector>(object_files | std::views::keys) };
}

vm::code::ValidProgram vm::code::ValidProgram::tryInsertCode(const code::CodeCollection& collection
) const {
	// @TODO: #1306 We could get rid of copying of the whole program.
	ValidProgram copy = *this;
	copy.insertCode(collection);
	return copy;
}

const vm::code::valid_type::ValidTypeMap& vm::code::ValidProgram::types() const {
	return type_context.getCurrentTypes();
}

const vm::code::TypeContext& vm::code::ValidProgram::getTypeContext() const { return type_context; }

const vm::ObjIdNameMap<vm::code::GlobalData>& vm::code::ValidProgram::globals() const {
	return globals_map;
}

const vm::ObjIdNameMap<vm::code::valid_function::ValidFunction>& vm::code::ValidProgram::functions(
) const {
	return function_map;
}

void vm::code::ValidProgram::insertCode(const CodeCollection& collection) {
	for (const auto& func: collection.functions) function_signatures.put(func.name, func.signature);
	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertExternalCFunctions(collection.external_c_functions);
	insertObjectFiles(collection.object_files);
	insertFFIFunctions(collection.ffi_functions);
	insertFunctions(collection.functions);
}

void vm::code::ValidProgram::insertTypes(const std::vector<TypeOfData>& new_types) {
	type_context.insertAndValidate(new_types, function_signatures);

	// Verify `assert_size` annotations against the computed layout (assuming 8-byte pointers).
	for (const auto& type: new_types) {
		auto data = getTypeKind<DataType>(type);
		if (!data.has_value() || !data->assert_size.has_value()) continue;

		auto actual = usize(
			type_context.getCurrentTypes().at(data->name)->getSize().assumePointerSize(Bytes(8))
		);
		if (actual != data->assert_size.value())
			throw TypeSizeAssertError(data->name, data->assert_size.value(), actual);
	}
}

void vm::code::ValidProgram::insertGlobals(const std::vector<GlobalData>& new_globals) {
	const auto& types = type_context.getCurrentTypes();

	for (const auto& global: new_globals) {
		if (globals_map.contains(global.name))
			throw DuplicatedGlobalDataError(global, *globals_map.at(global.name));
		if (!types.contains(global.type)) throw UnknownTypeError(opargs::Type(global.type));
		if (global.ctor_name.has_value() && global.initial_value.has_value())
			throw GlobalCtorAndInitialValueConflictError(global.name);
		if (global.ctor_name.has_value() && !function_signatures.contains(global.ctor_name.value()))
			throw MissingGlobalCtorDtorError(true, global.ctor_name.value(), global.name);
		if (global.dtor_name.has_value() && !function_signatures.contains(global.dtor_name.value()))
			throw MissingGlobalCtorDtorError(false, global.dtor_name.value(), global.name);
		if (global.initial_value.has_value()) {
			auto type_it = types.at(global.type);
			detail::validateInitialValue(
				global.initial_value.value(), type_it->getID(), types, global.name
			);
		}
		globals_map.insert(global, global.name);
	}
}

void vm::code::ValidProgram::insertFunctions(const std::vector<Function>& new_functions) {
	if (new_functions.empty()) return;

	for (const auto& func: new_functions) {
		if (function_map.contains(func.name))
			throw DuplicatedFunctionError(func, function_map.at(func.name)->toNormal());

		auto validated_function = detail::validateAndExtractReachableCode(
			type_context.getCurrentTypes(),
			globals_map,
			function_signatures,
			ext_c_function_map,
			ffi_function_map,
			func
		);
		function_map.insert(validated_function, validated_function.name);
	}
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

void vm::code::ValidProgram::insertFFIFunctions(const std::vector<FFIFunction>& new_functions) {
	if (new_functions.empty()) return;

	auto is_cptr = [](const valid_type::ValidType& tp) {
		return tp.isKind<valid_type::finalized::Opaque>() && tp.getName() == base::StrID("cptr");
	};
	auto is_ffi_primitive = [](const valid_type::ValidType& tp) {
		if (auto primitive = tp.maybeGetKindAs<valid_type::finalized::Primitive>()) {
			auto size = usize(primitive.value()->size);
			// `f32`/`f64` map to C `float`/`double` in `buildFFIType`; any other size under
			// these names would be silently misclassified, so reject it here.
			if (tp.getName() == base::StrID("f32")) return size == 4;
			if (tp.getName() == base::StrID("f64")) return size == 8;
			return size == 1 || size == 2 || size == 4 || size == 8;
		}
		return false;
	};
	auto validate_ffi_type = [&](const Identifier& type_name) {
		const auto& types = type_context.getCurrentTypes();

		auto tp = types.atMaybe(type_name);
		if (!tp) throw UnknownTypeError(opargs::Type(type_name));

		if (is_ffi_primitive(*tp.value()) || is_cptr(*tp.value())) return;

		if (auto structure = tp.value()->maybeGetKindAs<valid_type::finalized::Structure>()) {
			// Classes and interfaces are not C-compatible, only plain data structures whose
			// every field is a primitive or a `cptr`.
			if (structure.value()->inheritance_metadata.has_value())
				throw FFIUnsupportedTypeError(*tp.value());
			for (const auto& field: structure.value()->fields) {
				const auto& field_type = *types.at(field.type);
				if (!is_ffi_primitive(field_type) && !is_cptr(field_type))
					throw FFIUnsupportedTypeError(*tp.value());
			}

			// Verify the VM layout of the structure matches the C ABI layout libffi will use
			// (the VM packs fields, C inserts alignment padding).
			ffi_detail::FFITypeStorage storage;
			ffi_type* struct_type = ffi_detail::buildFFIType(*tp.value(), types, storage);

			std::vector<size_t> c_offsets(structure.value()->fields.size());
			CORE_ASSERT(
				ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, c_offsets.data()) == FFI_OK,
				"ffi_get_struct_offsets failed"
			);

			auto to_bytes = [](const valid_type::TypeSize& size) {
				return usize(size.assumePointerSize(Bytes(16)));
			};
			if (to_bytes(tp.value()->getSize()) != struct_type->size)
				throw FFIStructLayoutMismatchError(*tp.value());
			for (const auto& [field, c_offset]:
			     std::views::zip(structure.value()->fields, c_offsets))
				if (to_bytes(field.offset) != c_offset)
					throw FFIStructLayoutMismatchError(*tp.value());

			return;
		}

		throw FFIUnsupportedTypeError(*tp.value());
	};

	auto resolve_symbol = [&](const FFIFunction& func) -> void (*)() {
		for (const auto& [file, library]: object_files) {
			if_opt_some(library->maybeFindSymbol(func.name.str.str().c_str()), symbol) {
				return reinterpret_cast<void (*)()>(symbol);
			}
		}
		throw FFIUnknownSymbolError(func);
	};

	for (const auto& new_func: new_functions) {
		if (ffi_function_map.contains(new_func.name))
			throw DuplicatedFFIFunctionError(new_func, *ffi_function_map.at(new_func.name));

		if (new_func.signature.result_types.size()) {
			CORE_ASSERT(
				new_func.signature.result_types.size() == 1, "FFI functions return only one type"
			);
			validate_ffi_type(new_func.signature.result_types[0]);
		}

		for (const auto& type: new_func.signature.parameters) validate_ffi_type(type);

		FFIFunction resolved = new_func;
		resolved.symbol      = resolve_symbol(new_func);
		ffi_function_map.insert(resolved, resolved.name);
	}
}

void vm::code::ValidProgram::insertObjectFiles(const std::vector<fs::File>& new_files) {
	for (const auto& file: new_files) {
		if (object_files.contains(file)) continue;

		auto library = native::DynamicLibrary::tryFromFile(file.getFilePath().string().c_str());
		if (!library.has_value()) throw FFIObjectFileError(file, library.error());

		object_files.emplace(
			file, std::make_shared<native::DynamicLibrary>(std::move(library).value())
		);
	}
}

const vm::ObjIdNameMap<vm::code::ExternalCFunction>& vm::code::ValidProgram::extCFunctions() const {
	return ext_c_function_map;
}

const vm::ObjIdNameMap<vm::code::FFIFunction>& vm::code::ValidProgram::ffiFunctions() const {
	return ffi_function_map;
}

const std::unordered_map<fs::File, std::shared_ptr<vm::native::DynamicLibrary>>& vm::code::
	ValidProgram::objectFiles() const {
	return object_files;
}
