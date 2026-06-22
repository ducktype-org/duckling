#include "program_lowering_context.hpp"

#include "debug_info_utils.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"

#include <backends/dvm/dvm_internal_fwd.hpp>
#include <debug_info/debug_info_builder.hpp>
#include <tsl/type_layout.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <logger/logger.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>

#include <algorithm>
#include <ranges>

using namespace compiler::backend_vm::internal;

compiler::backend_vm::internal::ProgramLoweringContext::ProgramLoweringContext(
	query::Context& query_ctx, bool build_debug_info, bool is_comp_time_lowering
):
	  query_ctx_for_errors(&query_ctx),
	  is_comp_time_lowering(is_comp_time_lowering),
	  debug_info_builder(
		  (build_debug_info ? debug_info::DebugInfoBuilder(
								  debug_info::Target::DBC, debug_info::SourcePositionsType::PstHash
							  )
                            : base::Optional<debug_info::DebugInfoBuilder>{})
	  ) {}

CRef<vm::code::TypeOfData> ProgramLoweringContext::keepVMType(vm::code::TypeOfData dvm_type) {
	auto type_name      = typeName(dvm_type);
	auto [it, inserted] = type_storage.dvm_types.put(type_name, std::move(dvm_type));
	if (inserted) lowered_type_order.push_back(type_name);
	return &it->second;
}

base::Optional<CRef<vm::code::TypeOfData>> ProgramLoweringContext::lowerAndKeepTslType(
	CRef<tsl::TypeLayout> layout
) {
	if (auto maybe_name = type_storage.tsl_type_to_dvm_type_name.atMaybe(layout))
		return CRef(&type_storage.dvm_types.at(**maybe_name));

	auto maybe_dvm_type = lowerTslTypeInternal(layout);
	if (!maybe_dvm_type.has_value()) return {};

	vm::code::TypeOfData dvm_type  = *maybe_dvm_type;
	base::StrID          type_name = vm::code::typeName(dvm_type);

	IF_BUILD_TYPE_DEV({
		auto maybe_type = type_storage.dvm_types.atMaybe(type_name);
		CORE_ASSERT(
			!maybe_type.has_value() || **maybe_type == dvm_type, "Type mismatch in type lowering"
		);
	});

	type_storage.tsl_type_to_dvm_type_name.put(layout, type_name);
	type_storage.dvm_types.put(type_name, std::move(dvm_type));
	lowered_type_order.push_back(type_name);

	CORE_DEV_LOG(REPL, "[DEBUG] lowered_type_order size=", lowered_type_order.size(), "\n");
	for (const auto& lowered_type: lowered_type_order)
		CORE_DEV_LOG(REPL, "  type: ", lowered_type, "\n");


	if_opt_some(debug_info_builder, builder) {
		builder.addType(type_name.str(), layout->getSourceType().toString());
	}

	return CRef(&type_storage.dvm_types.at(type_name));
}

compiler::backend_vm::LoweredEntitiesSnapshot ProgramLoweringContext::captureLoweredEntitiesSnapshot(
) const {
	return { lowered_type_order.size(),
		     lowered_global_order.size(),
		     lowered_function_order.size(),
		     extra_bytecode_functions.size() };
}

const vm::code::TypeOfData& ProgramLoweringContext::getOrInsertPointerType(
	const vm::code::TypeOfData& pointee_type
) {
	base::StrID pointee_name = vm::code::typeName(pointee_type);
	auto        pointer_name = base::StrID(base::strConcat("ptr_", pointee_name));

	if (auto maybe_type = type_storage.dvm_types.atMaybe(pointer_name)) return **maybe_type;

	vm::code::PointerType pointer_type(pointer_name, pointee_name);
	type_storage.dvm_types.put(pointer_name, pointer_type);
	lowered_type_order.push_back(pointer_name);
	return type_storage.dvm_types.at(pointer_name);
}

const DVMPlace& ProgramLoweringContext::getLirGlobal(CRef<lir::LIRGlobal> lir_global) {
	if (auto maybe_global = global_name_to_dvm.atMaybe(lir_global->mangled_name))
		return **maybe_global;
	else {
		const vm::code::TypeOfData& global_type = **lowerAndKeepTslType(lir_global->layout);

		global_name_to_dvm.put(
			lir_global->mangled_name,
			DVMPlace(lir_global->mangled_name, global_type, DVMPlace::AccessKind::Direct)
		);

		return global_name_to_dvm.at(lir_global->mangled_name);
	}
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
	for (const auto& global: bytecode.global_data) {
		if (!global_name_to_dvm_data.atMaybe(global.name).has_value())
			lowered_global_order.push_back(global.name);
		global_name_to_dvm_data.put(global.name, global);
	}

	for (const auto& ext_func: bytecode.external_c_functions)
		extern_c_functions.put(ext_func.name.str, ext_func);

	extra_bytecode_functions.insert(
		extra_bytecode_functions.end(), bytecode.functions.begin(), bytecode.functions.end()
	);
}

vm::code::CodeCollection ProgramLoweringContext::collectNewCodeSince(
	const compiler::backend_vm::LoweredEntitiesSnapshot& snapshot
) const {
	auto collect_types_since = [&](usize start_index) {
		CORE_ASSERT(
			start_index <= lowered_type_order.size(),
			"Requested lowered types from an out-of-range start index"
		);
		std::vector<vm::code::TypeOfData> result;
		result.reserve(lowered_type_order.size() - start_index);
		for (usize i = start_index; i < lowered_type_order.size(); ++i) {
			const auto& type_name = lowered_type_order[i];
			result.push_back(type_storage.dvm_types.at(type_name));
		}
		return result;
	};

	auto collect_functions_since = [&](usize start_index) {
		CORE_ASSERT(
			start_index <= lowered_function_order.size(),
			"Requested lowered functions from an out-of-range start index"
		);
		std::vector<vm::code::Function> result;
		result.reserve(lowered_function_order.size() - start_index);
		for (usize i = start_index; i < lowered_function_order.size(); ++i) {
			const auto& func_name = lowered_function_order[i];
			result.push_back(dvm_functions_by_name.at(func_name));
		}
		return result;
	};

	auto collect_globals_since = [&](usize start_index) {
		CORE_ASSERT(
			start_index <= lowered_global_order.size(),
			"Requested lowered globals from an out-of-range start index"
		);
		std::vector<vm::code::GlobalData> result;
		result.reserve(lowered_global_order.size() - start_index);
		for (usize i = start_index; i < lowered_global_order.size(); ++i) {
			const auto& global_name = lowered_global_order[i];
			result.push_back(global_name_to_dvm_data.at(global_name));
		}
		return result;
	};

	auto collect_extra_functions_since = [&](usize start_index) {
		CORE_ASSERT(
			start_index <= extra_bytecode_functions.size(),
			"Requested extra bytecode functions from an out-of-range start index"
		);
		auto offset = static_cast<std::ptrdiff_t>(start_index);
		return std::vector<vm::code::Function>{ extra_bytecode_functions.begin() + offset,
			                                    extra_bytecode_functions.end() };
	};

	vm::code::CodeCollection collection;
	collection.types       = collect_types_since(snapshot.lowered_type_count);
	collection.global_data = collect_globals_since(snapshot.lowered_global_count);
	collection.functions   = collect_functions_since(snapshot.lowered_function_count);
	auto extra             = collect_extra_functions_since(snapshot.extra_bytecode_function_count);
	collection.functions.insert(collection.functions.end(), extra.begin(), extra.end());
	return collection;
}

const vm::code::GlobalData& ProgramLoweringContext::lowerAndKeepLirGlobal(
	const lir::LIRGlobalData& lir_global
) {
	if (auto maybe_global = global_name_to_dvm_data.atMaybe(lir_global.global.mangled_name))
		return **maybe_global;

	auto& global_type = **lowerAndKeepTslType(lir_global.global.layout);
	auto& dvm_global_place
		= getLirGlobal(&lir_global.global);  // Ensure the global is added to the map.

	using vm::code::Identifier;

	base::Optional<Identifier> ctor_name;
	base::Optional<Identifier> dtor_name;

	variant_match(lir_global.data_initialization) {
		variant_case(lir::LIRGlobalData::CTorDtorPair, ctor_dtor_pair) {
			if (ctor_dtor_pair.global_ctor.has_value()) {
				lowerAndKeepLirFunction(ctor_dtor_pair.global_ctor.value());
				ctor_name = Identifier(ctor_dtor_pair.global_ctor.value()->mangled_name);
			}

			if (ctor_dtor_pair.global_dtor.has_value()) {
				lowerAndKeepLirFunction(ctor_dtor_pair.global_dtor.value());
				dtor_name = Identifier(ctor_dtor_pair.global_dtor.value()->mangled_name);
			}
		}
		variant_case(ctv::CompileTimeValue, ctv_initial_value) {
			// @TODO: #1553 we create mini-ctors for global variables with CTV initializers for now.
			// Ideally, we should add proper support for immediate value initializers in the DVM and
			// avoid this workaround.

			auto mini_ctor_name
				= base::StrID(base::strConcat(lir_global.global.mangled_name.strView(), "_ctv_ctor")
			    );

			auto mini_ctor = createMiniGlobalCtorFromCTV(
				*this,
				lir_global.global.layout,
				global_type,
				ctv_initial_value,
				mini_ctor_name,
				dvm_global_place
			);
			extra_bytecode_functions.push_back(std::move(mini_ctor));
			ctor_name = Identifier(mini_ctor_name);
		}
		variant_default {
			CORE_PANIC("Unhandled LIRGlobalData initial value type in lowerAndKeepLirGlobal");
		}
	}

	// @TODO: #1553 add a isConst to DVM and initial values, add source position to
	// GlobalVariables
	vm::code::GlobalData global_data{};
	global_data.name      = lir_global.global.mangled_name;
	global_data.type      = typeName(global_type);
	global_data.ctor_name = ctor_name;
	global_data.dtor_name = dtor_name;

	global_name_to_dvm_data.put(lir_global.global.mangled_name, global_data);
	lowered_global_order.push_back(lir_global.global.mangled_name);
	return global_name_to_dvm_data.at(lir_global.global.mangled_name);
}

const vm::code::Function& ProgramLoweringContext::lowerAndKeepLirFunction(
	CRef<lir::Function> lir_function
) {
	CORE_ASSERT(not lir_function->ignore_on_dvm, "Lowering a function that should not be lowered.");

	if (auto maybe_name = lir_function_to_name.atMaybe(lir_function))
		return dvm_functions_by_name.at(**maybe_name);

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
			opt_none { func_ctx.registerFunctionLocal(&param); }
		}
	}

	for (const auto& block_ref: lir_function->block_order) {
		func_ctx.beginBlock(block_ref);
		for (const auto& instruction: block_ref->instructions)
			func_ctx.pushInstruction(instruction);
		func_ctx.pushTerminator(block_ref->terminator);
	}

	auto       dvm_function  = std::move(func_ctx).finish();
	const auto function_name = dvm_function.name.str;
	dvm_functions_by_name.put(function_name, std::move(dvm_function));
	lir_function_to_name.put(lir_function, function_name);
	const auto& desired_function = dvm_functions_by_name.at(function_name);
	lowered_function_order.push_back(function_name);

	CORE_DEV_LOG(REPL, "[DEBUG] lowered_function_order size=", lowered_function_order.size(), "\n");
	for (const auto& lowered_function_name: lowered_function_order)
		CORE_DEV_LOG(REPL, "  function: ", lowered_function_name, "\n");

	return desired_function;
}

void ProgramLoweringContext::insertExternCFunction(const vm::code::ExternalCFunction& extern_func) {
	extern_c_functions.put(extern_func.name.str, extern_func);
}

base::Optional<vm::code::TypeOfData> ProgramLoweringContext::lowerTslTypeInternal(
	CRef<tsl::TypeLayout> layout
) {
	variant_match(layout->getVariant()) {
		variant_case_novalue(tsl::EmptyTypeLayout) { return {}; }
		variant_case_novalue(tsl::IntegralTypeLayout) {
			Bits bits = layout->getSize();
			if (bits == Bits{ 1 }) bits = Bits{ 8 };  // Boolean edge-case.
			Bytes       bytes = base::bits2bytes(bits);
			std::string name  = "i" + base::toString(bits.asInt());
			return vm::code::PrimitiveType(base::StrID(name), bytes);
		}
		variant_case_novalue(tsl::FloatTypeLayout) {
			Bits        bits  = layout->getSize();
			Bytes       bytes = base::bits2bytes(bits);
			std::string name  = "f" + base::toString(bits.asInt());
			return vm::code::PrimitiveType(base::StrID(name), bytes);
		}
		variant_case_novalue(tsl::MetaTypeLayout) {
			return vm::code::OpaqueType(base::StrID("opaque_ptr"), Bytes{ 8 });
		}
		variant_case(tsl::PointerTypeLayout, pointer_layout) {
			const vm::code::TypeOfData& pointee_type
				= **lowerAndKeepTslType(pointer_layout.getPointee());
			switch (pointer_layout.getPointerKind()) {
			case tsl::PointerTypeLayout::PointerKind::SinglePointer: {
				auto pointer_type_name = base::strConcat("ptr_", typeName(pointee_type));
				return vm::code::PointerType(base::StrID(pointer_type_name), typeName(pointee_type));
			}
			case tsl::PointerTypeLayout::PointerKind::ManyPointer: {
				// Many pointer is a pointer to a dynamic table of the pointee type.
				auto dyntable_type_name = base::strConcat("dyntable_", typeName(pointee_type));
				vm::code::DynamicTableType dyntable_type(
					base::StrID(dyntable_type_name), typeName(pointee_type)
				);
				// Ensure the dynamic table type is stored in the context.
				keepVMType(dyntable_type);
				auto pointer_type_name = base::strConcat("ptr_", dyntable_type_name);
				return vm::code::PointerType(
					base::StrID(pointer_type_name), typeName(dyntable_type)
				);
			}
			case tsl::PointerTypeLayout::PointerKind::CPointer: {
				query_ctx_for_errors.value()->logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"CPointer types are not supported in DVM code generation yet: ",
						layout->toStringDefinition(*query_ctx_for_errors.value())
					),
					""
				));
				query::throwFailed();
				break;
			}
			default:
				CORE_PANIC("All cases should be covered.");
			}
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
				const auto field_layout = class_layout.getFieldLayoutOfLayoutIndex(i);
				const vm::code::TypeOfData& vm_field_type = **lowerAndKeepTslType(field_layout);
				fields.emplace_back(base::StrID(base::strConcat("_", i)), typeName(vm_field_type));
			}

			return vm::code::DataType{
				base::StrID(class_layout.getMangledName()),
				std::move(fields),
			};
		}
		variant_case(tsl::StaticArrayTypeLayout, array_layout) {
			const auto                  element_layout  = array_layout.getElementLayout();
			const vm::code::TypeOfData& vm_element_type = **lowerAndKeepTslType(element_layout);
			const usize                 num_elements    = array_layout.getElementCount();
			auto                        array_type_name
				= base::strConcat("arr_", typeName(vm_element_type), "_", num_elements);

			return vm::code::FixedSizeTableType{
				base::StrID(array_type_name),
				typeName(vm_element_type),
				num_elements,
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
				""
			));
			query::throwFailed();
		}
	}
	CORE_UNREACHABLE();
}

vm::code::CodeCollection ProgramLoweringContext::produceCodeCollection() {
	auto collection      = vm::code::CodeCollection();
	collection.functions = std::ranges::to<std::vector>(dvm_functions_by_name | std::views::values);
	collection.functions.insert(
		collection.functions.end(), extra_bytecode_functions.begin(), extra_bytecode_functions.end()
	);

	// Sort globals and functions by their mangled names to ensure deterministic output, which is
	// important for reproducibility. This also should guarantee that the order of functions and
	// globals in the resulting DVM module is deterministic, which can be important for debugging
	// and testing.
	std::ranges::sort(
		collection.functions,
		[](const vm::code::Function& lhs, const vm::code::Function& rhs) {
			return lhs.name.str.strView() < rhs.name.str.strView();
		}
	);
	collection.global_data.reserve(lowered_global_order.size());
	for (const auto& global_name: lowered_global_order)
		collection.global_data.push_back(global_name_to_dvm_data.at(global_name));

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

	return collection;
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

bool compiler::backend_vm::internal::ProgramLoweringContext::isCompTimeLowering() const {
	return is_comp_time_lowering;
}
