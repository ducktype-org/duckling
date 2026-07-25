#include "valid_program.hpp"

#include "errors.hpp"

#include <string_id/string_id.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/ffi_type_builder.hpp>
#include <vm/bytecode/validator/function_validator.hpp>
#include <vm/bytecode/validator/initial_value.hpp>
#include <vm/bytecode/validator/type_validator.hpp>

#include <algorithm>

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

vm::code::ValidProgram vm::code::ValidProgram::tryInsertCode(
	const code::CodeCollection& collection, api::ExecutionConfig config
) const {
	// @TODO: #1306 We could get rid of copying of the whole program.
	ValidProgram copy = *this;
	copy.insertCode(collection, config);
	return copy;
}

vm::code::valid_function::ValidFunction vm::code::ValidProgram::validateExpr(
	CRef<SafeVMThread> thread, const code::Function& expr
) const {
	return detail::validateAndExtractReachableCode(
		type_context.getCurrentTypes(),
		globals_map,
		function_signatures,
		ext_c_function_map,
		flag_context,
		ffi_function_map,
		expr,
		thread
	);
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

void vm::code::ValidProgram::insertCode(
	const CodeCollection& collection, api::ExecutionConfig config
) {
	for (const auto& func: collection.functions) function_signatures.put(func.name, func.signature);
	insertTypes(collection.types);
	insertGlobals(collection.global_data);
	insertExternalCFunctions(collection.external_c_functions);
	insertObjectFiles(collection.object_files);
	insertFFIFunctions(collection.ffi_functions);
	insertFunctions(collection.functions, config);
}

void vm::code::ValidProgram::insertTypes(const std::vector<TypeOfData>& new_types) {
	type_context.insertAndValidate(new_types, function_signatures);

	// Verify `assert_size` annotations against the computed layout.
	for (const auto& type: new_types) {
		auto data = getTypeKind<DataType>(type);
		if (!data.has_value() || !data->assert_size.has_value()) continue;

		// A size that depends on the pointer width has no single value to assert against (the
		// safe interpreter uses 16-byte pointers, C uses 8), so it is rejected.
		const auto& size = type_context.getCurrentTypes().at(data->name)->getSize();
		if (size.assumePointerSize(Bytes(8)) != size.assumePointerSize(Bytes(16)))
			throw TypeSizeAssertPointerDependentError(data->name);

		auto actual = usize(size.assumePointerSize(Bytes(8)));
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

void vm::code::ValidProgram::insertFunctions(
	const std::vector<Function>& new_functions, api::ExecutionConfig config
) {
	if (new_functions.empty()) return;
	flag_context.insertAndValidate(new_functions, globals_map, ext_c_function_map, config);
	for (const auto& func: new_functions) {
		if (function_map.contains(func.name))
			throw DuplicatedFunctionError(func, function_map.at(func.name)->toNormal());

		auto validated_function = detail::validateAndExtractReachableCode(
			type_context.getCurrentTypes(),
			globals_map,
			function_signatures,
			ext_c_function_map,
			flag_context,
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

	auto validate_ffi_type = [&](const Identifier& type_name) {
		const auto& types = type_context.getCurrentTypes();

		auto tp = types.atMaybe(type_name);
		if (!tp) throw UnknownTypeError(opargs::Type(type_name));

		// C has no by-value arrays: a fixed-size table can only cross the FFI boundary as a
		// structure field.
		if (tp.value()->isKind<valid_type::finalized::FixedSizeTable>())
			throw FFITableByValueError(*tp.value());

		if (!tp.value()->isFFICompliant()) {
			// A dedicated message for the packed case (see the compliance computation in
			// `ValidType::finalize` for the rationale).
			if (auto structure = tp.value()->maybeGetKindAs<valid_type::finalized::Structure>();
			    structure.has_value() && structure.value()->packed)
				throw FFIPackedTypeError(*tp.value());
			throw FFIUnsupportedTypeError(*tp.value());
		}

		if (auto structure = tp.value()->maybeGetKindAs<valid_type::finalized::Structure>()) {
			// Building the descriptor materializes one `ffi_type*` per element, nested structure
			// descriptors included, so the total is capped before anything is built.
			if (ffi_detail::totalFFIDescriptorElementCount(*tp.value(), types)
			    > ffi_detail::MAX_FLATTENED_FFI_ELEMENTS)
				throw FFIUnsupportedTypeError(*tp.value());

			// Fixed-size table fields are flattened in the libffi descriptor, so a field can
			// span several elements; each VM field is compared against its first one. The sum
			// cannot overflow: it is bounded by the capped total above.
			usize element_count = 0;
			for (const auto& field: structure.value()->fields)
				element_count += ffi_detail::flattenedFFIElementCount(*types.at(field.type), types);

			// Verify the VM layout of the structure matches the C ABI layout libffi will use.
			// FFI-compliant structures follow the C layout rules, so this is a defensive check.
			ffi_detail::FFITypeStorage storage;
			ffi_type* struct_type = ffi_detail::buildFFIType(*tp.value(), types, storage);

			std::vector<size_t> c_offsets(element_count);
			ffi_status          offsets_status
				= ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, c_offsets.data());
			if (offsets_status != FFI_OK) throw FFIStructLayoutMismatchError(*tp.value());

			// The pointer size never enters here (FFI-compliant fields have pointer-independent
			// sizes); `Bytes(8)` only keeps this consistent with the `assert_size` check in
			// `insertTypes`.
			auto to_bytes = [](const valid_type::TypeSize& size) {
				return usize(size.assumePointerSize(Bytes(8)));
			};
			if (to_bytes(tp.value()->getSize()) != struct_type->size)
				throw FFIStructLayoutMismatchError(*tp.value());
			usize flattened_index = 0;
			for (const auto& field: structure.value()->fields) {
				if (to_bytes(field.offset) != c_offsets[flattened_index])
					throw FFIStructLayoutMismatchError(*tp.value());
				flattened_index
					+= ffi_detail::flattenedFFIElementCount(*types.at(field.type), types);
			}
		}
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

		// The compiler and the runtime handle at most one result; user bytecode must not get
		// past validation with more.
		if (new_func.signature.result_types.size() > 1) throw FFIMultipleResultsError(new_func);
		if (!new_func.signature.result_types.empty())
			validate_ffi_type(new_func.signature.result_types[0]);

		for (const auto& type: new_func.signature.parameters) validate_ffi_type(type);

		FFIFunction resolved = new_func;
		resolved.symbol      = resolve_symbol(new_func);
		ffi_function_map.insert(resolved, resolved.name);
	}
}

void vm::code::ValidProgram::insertObjectFiles(const std::vector<std::string>& new_files) {
	for (const auto& file: new_files) {
		if (std::ranges::any_of(object_files, [&](const auto& entry) {
				return entry.first == file;
			}))
			continue;

		auto library = native::DynamicLibrary::tryFromFile(file.c_str());
		if (!library.has_value()) throw FFIObjectFileError(file, library.error());

		object_files.emplace_back(
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

const std::vector<std::pair<std::string, std::shared_ptr<vm::native::DynamicLibrary>>>& vm::code::
	ValidProgram::objectFiles() const {
	return object_files;
}
