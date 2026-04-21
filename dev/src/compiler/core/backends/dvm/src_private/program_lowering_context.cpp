#include "program_lowering_context.hpp"

#include "debug_info_utils.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"

#include <backends/dvm/dvm_internal_fwd.hpp>
#include <debug_info/debug_info_builder.hpp>
#include <tsl/type_layout.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>

#include <algorithm>
#include <ranges>

using namespace compiler::backend_vm::internal;

compiler::backend_vm::internal::ProgramLoweringContext::ProgramLoweringContext(
	query::Context& query_ctx, bool build_debug_info
):

	  query_ctx_for_errors(&query_ctx),
	  debug_info_builder(
		  (build_debug_info ? debug_info::DebugInfoBuilder(
								  debug_info::Target::DBC, debug_info::SourcePositionsType::PstHash
							  )
                            : base::Optional<debug_info::DebugInfoBuilder>{})
	  ) {}

const vm::code::TypeOfData& ProgramLoweringContext::lowerAndKeepTslType(CRef<tsl::TypeLayout> layout
) {
	if (auto maybe_name = type_storage.tsl_type_to_dvm_type_name.atMaybe(layout))
		return type_storage.dvm_types.at(**maybe_name);

	vm::code::TypeOfData dvm_type  = lowerTslTypeInternal(layout);
	base::StrID          type_name = vm::code::typeName(dvm_type);


	IF_BUILD_TYPE_DEV({
		auto maybe_dvm_type = type_storage.dvm_types.atMaybe(type_name);
		CORE_ASSERT(
			!maybe_dvm_type.has_value() || **maybe_dvm_type == dvm_type,
			"Type mismatch in type lowering"
		);
	});

	type_storage.tsl_type_to_dvm_type_name.put(layout, type_name);
	type_storage.dvm_types.put(type_name, std::move(dvm_type));

	if_opt_some(debug_info_builder, builder) {
		builder.addType(type_name.str(), layout->getSourceType().toString());
	}

	return type_storage.dvm_types.at(type_name);
}

const vm::code::TypeOfData& ProgramLoweringContext::getOrInsertPointerType(
	const vm::code::TypeOfData& pointee_type
) {
	base::StrID pointee_name = vm::code::typeName(pointee_type);
	auto        pointer_name = base::StrID(base::strConcat("ptr_", pointee_name));

	if (auto maybe_type = type_storage.dvm_types.atMaybe(pointer_name)) return **maybe_type;

	vm::code::PointerType pointer_type(pointer_name, pointee_name);
	type_storage.dvm_types.put(pointer_name, pointer_type);
	return type_storage.dvm_types.at(pointer_name);
}

const DVMPlace& ProgramLoweringContext::getLirGlobal(CRef<lir::LIRGlobal> global) const {
	if (auto maybe_global = global_name_to_dvm.atMaybe(global->mangled_name))
		return **maybe_global;
	else
		CORE_PANIC("LIR global not previously lowered: ", global->mangled_name);
}

const vm::code::ExternalCFunction& ProgramLoweringContext::getExternCFunction(
	const base::StrID& func_name
) const {
	if (auto maybe_ext_func = extern_c_functions.atMaybe(func_name))
		return **maybe_ext_func;
	else
		CORE_PANIC("Extern C function not found: ", func_name);
}

void ProgramLoweringContext::insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode) {
	for (const auto& global: bytecode.global_data) global_name_to_dvm_data.put(global.name, global);

	for (const auto& ext_func: bytecode.external_c_functions)
		extern_c_functions.put(ext_func.name.str, ext_func);

	extra_bytecode_functions.insert(
		extra_bytecode_functions.end(), bytecode.functions.begin(), bytecode.functions.end()
	);
}

const vm::code::GlobalData& ProgramLoweringContext::lowerAndKeepLirGlobal(
	const lir::LIRGlobal&               lir_global,
	base::Optional<CRef<lir::Function>> global_ctor,
	base::Optional<CRef<lir::Function>> global_dtor
) {
	if (auto maybe_global = global_name_to_dvm_data.atMaybe(lir_global.mangled_name))
		return **maybe_global;

	auto global_type = lowerAndKeepTslType(lir_global.layout);

	// Register the global variable itself before inserting ctor/dtor to handle
	// recursive references.
	global_name_to_dvm.put(
		lir_global.mangled_name,
		DVMPlace(lir_global.mangled_name, global_type, DVMPlace::AccessKind::Direct)
	);

	using vm::code::Identifier;

	base::Optional<Identifier> ctor_name;
	base::Optional<Identifier> dtor_name;

	if (global_ctor.has_value()) {
		lowerAndKeepLirFunction(global_ctor.value());
		ctor_name = Identifier(global_ctor.value()->mangled_name);
	}
	if (global_dtor.has_value()) {
		lowerAndKeepLirFunction(global_dtor.value());
		dtor_name = Identifier(global_dtor.value()->mangled_name);
	}

	// @TODO: #1553 add a isConst to DVM and initial values, add source position to
	// GlobalVariables
	vm::code::GlobalData global_data{};
	global_data.name      = lir_global.mangled_name;
	global_data.type      = typeName(global_type);
	global_data.ctor_name = ctor_name;
	global_data.dtor_name = dtor_name;

	global_name_to_dvm_data.put(lir_global.mangled_name, global_data);
	return global_name_to_dvm_data.at(lir_global.mangled_name);
}

const vm::code::Function& ProgramLoweringContext::lowerAndKeepLirFunction(
	CRef<lir::Function> lir_function
) {
	if (auto maybe_lowered = lir_function_to_dvm.atMaybe(lir_function)) return **maybe_lowered;

	auto func_result_type = lowerAndKeepTslType(lir_function->return_type_layout);
	std::vector<vm::code::TypeOfData> func_param_types;
	for (const auto& param_layout: lir_function->parameter_layouts)
		func_param_types.push_back(lowerAndKeepTslType(param_layout));


	base::Optional<debug_info::FunctionBuilder> function_di_builder_opt;
	if_opt_some(debug_info_builder, builder) {
		function_di_builder_opt.emplace(builder.beginFunction(
			lir_function->mangled_name.str(),
			lir_function->metadata.source_code_name.map([](auto str_id) { return str_id.str(); }),
			lir_function->metadata.position.map(mapDIPosition)
		));
	}


	auto func_ctx = FunctionLoweringContext{ *this,
		                                     lir_function->mangled_name,
		                                     lir_function->return_type_layout,
		                                     lir_function->parameter_layouts,
		                                     std::move(function_di_builder_opt) };


	for (const auto& param: lir_function->local_list) {
		match_optional(param.parameter_index) {
			opt_some(_) func_ctx.registerFunctionParameter(&param);
			opt_none {
				// @TODO: #1656 Handle local variable inits properly.
				func_ctx.pushInit(&param);  // NOLINT(clang-diagnostic-deprecated-declarations)
			}
		}
	}

	for (const auto& block_ref: lir_function->block_order) {
		func_ctx.beginBlock(block_ref);
		for (const auto& instruction: block_ref->instructions)
			func_ctx.pushInstruction(instruction);
		func_ctx.pushTerminator(block_ref->terminator);
	}

	auto dvm_function = std::move(func_ctx).finish();
	lir_function_to_dvm.put(lir_function, dvm_function);
	return lir_function_to_dvm.at(lir_function);
}

void ProgramLoweringContext::insertExternCFunction(const vm::code::ExternalCFunction& extern_func) {
	extern_c_functions.put(extern_func.name.str, extern_func);
}

vm::code::TypeOfData ProgramLoweringContext::lowerTslTypeInternal(CRef<tsl::TypeLayout> layout) {
	variant_match(layout->getVariant()) {
		variant_case_novalue(tsl::EmptyTypeLayout) {
			return vm::code::PrimitiveType(base::StrID("void"), 1);
		}
		variant_case_novalue(tsl::IntegralTypeLayout) {
			auto bits = usize(layout->getSize());
			if (bits == 1) bits = 8;  // Boolean case.
			if (bits % 8 != 0) CORE_PANIC("Integral type size not divisible by 8");
			usize       bytes = bits / 8;
			std::string name  = "i" + std::to_string(bits);

			return vm::code::PrimitiveType(base::StrID(name), bytes);
		}
		variant_case_novalue(tsl::FloatTypeLayout) {
			auto bits = usize(layout->getSize());
			CORE_ASSERT(
				bits == 16 || bits == 32 || bits == 64 || bits == 80, "Invalid size of float: ", bits
			);
			usize       bytes = bits / 8;
			std::string name  = "f" + std::to_string(bits);

			return vm::code::PrimitiveType(base::StrID(name), bytes);
		}
		variant_case_novalue(tsl::MetaTypeLayout) {
			return vm::code::OpaqueType(base::StrID("opaque_ptr"), 8);
		}
		variant_case(tsl::PointerTypeLayout, pointer_layout) {
			auto pointee_type      = lowerAndKeepTslType(pointer_layout.getPointee());
			auto pointer_type_name = base::strConcat("ptr_", typeName(pointee_type));
			return vm::code::PointerType(base::StrID(pointer_type_name), typeName(pointee_type));
		}
		variant_case(tsl::ClassTypeLayout, class_layout) {
			std::vector<vm::code::Field> fields;
			const usize                  num_fields = class_layout.getNumSubLayouts();
			fields.reserve(num_fields);

			/*
			Class types are lowered to:
			type data: <name> {
			    _0: <type_of_field_0>
			    _1: <type_of_field_1>
			    _2: <type_of_field_2>
			}
			*/
			// @TODO: #2100 Change that to indexes.
			for (usize i{ 0 }; i < num_fields; i++) {
				const auto  field_layout  = class_layout.getFieldLayoutOfLayoutIndex(i);
				const auto& vm_field_type = lowerAndKeepTslType(field_layout);
				fields.emplace_back(base::StrID(base::strConcat("_", i)), typeName(vm_field_type));
			}

			return vm::code::DataType{
				base::StrID(class_layout.getMangledName()),
				std::move(fields),
			};
		}
		variant_default {
			CORE_ASSERT(
				query_ctx_for_errors.has_value(), "Query context must be set for error reporting"
			);
			query_ctx_for_errors.value()->logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				base::strConcat(
					"During TypeLayout lowering in DVM code generation - type not handled "
					"yet: ",
					layout->toStringDefinition(*query_ctx_for_errors.value())
				),
				base::Optional<dia::SourcePosition>()
			));
			query::throwFailed();
		}
	}
	CORE_UNREACHABLE();
}

std::expected<vm::code::CodeCollection, std::string> ProgramLoweringContext::validateAndProduceProgram(
) {
	auto collection      = vm::code::CodeCollection();
	collection.functions = std::ranges::to<std::vector>(lir_function_to_dvm | std::views::values);
	// Sort globals and functions by their mangled names to ensure deterministic output, which is
	// important for reproducibility. This also should guarantee that the order of functions and
	// globals in the resulting DVM module is deterministic, which can be important for debugging
	// and testing.
	std::ranges::sort(
		collection.functions,
		[](const vm::code::Function& lhs, const vm::code::Function& rhs) {
			return lhs.name.str < rhs.name.str;
		}
	);
	collection.functions.insert(
		collection.functions.end(), extra_bytecode_functions.begin(), extra_bytecode_functions.end()
	);
	collection.global_data
		= std::ranges::to<std::vector>(global_name_to_dvm_data | std::views::values);
	// Note, that this not only makes the output deterministic,
	// but also ensures that globals are ordered as they are declared in the source code
	// This is because they are sorted by mangled names base::StrID ids
	// This ids are incrementally generated during lirLowering, so the order of declaration is
	// preserved. This is true as long as the LIR globals are generated in the same order as the
	// source globals, which is currently the case. This is desirable behavior because we want to
	// generate globals in the order they appear in the file to prevent order initialization fiasco.
	// @TODO: #1431 Think about this and make it better if needed
	std::ranges::sort(
		collection.global_data,
		[](const vm::code::GlobalData& lhs, const vm::code::GlobalData& rhs) {
			return lhs.name.str < rhs.name.str;
		}
	);

	// Sorting types by their string identification to ensure deterministic output
	collection.types = std::ranges::to<std::vector>(type_storage.dvm_types | std::views::values);
	std::ranges::sort(
		collection.types,
		[](const vm::code::TypeOfData& lhs, const vm::code::TypeOfData& rhs) {
			return vm::code::typeName(lhs).strView() < vm::code::typeName(rhs).strView();
		}
	);

	collection.external_c_functions
		= std::ranges::to<std::vector>(extern_c_functions | std::views::values);

	try {
		auto valid = vm::code::ValidProgram::withBuiltins();
		valid      = valid.tryInsertCode(collection);
		return valid.produceValidCodeCollection();
	} catch (vm::code::ValidationError& e) { return std::unexpected(e.what()); }
}

base::Optional<debug_info::DebugInfo> compiler::backend_vm::internal::ProgramLoweringContext::buildDebugInfo(
) {
	if_opt_some(debug_info_builder, builder) {
		auto result        = std::move(builder).build();
		debug_info_builder = {};
		return result;
	}
	return {};
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(compiler::backend_vm::internal::ProgramLoweringContext);
