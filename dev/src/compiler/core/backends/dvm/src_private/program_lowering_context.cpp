#include "program_lowering_context.hpp"

#include "function_lowering_context.hpp"

#include <backends/dvm/dvm_forward_decl.hpp>

#include "base/collections/optional.hpp"

#include <vm/bytecode/bytecode.hpp>

using namespace compiler::backend_vm::internal;

const vm::code::TypeOfData& ProgramLoweringContext::lowerAndKeepTslType(
	CRef<tsl::TypeLayout> layout
) {
	if (tsl_type_to_dvm.contains(layout)) {
		return tsl_type_to_dvm.at(layout);
	} else {
		auto dvm_type = lowerTslTypeInternal(layout);
		tsl_type_to_dvm.put(layout, dvm_type);
		return tsl_type_to_dvm.at(layout);
	}
}

const DVMGlobal& ProgramLoweringContext::getLirGlobal(CRef<lir::LIRGlobal> global) const {
	if (auto maybe_global = lir_global_to_dvm.atMaybe(global))
		return **maybe_global;
	else
		CORE_PANIC("LIR global not previously lowered: ", global->mangled_name);
}

const vm::code::GlobalData& ProgramLoweringContext::lowerAndKeepLirGlobal(
	const lir::LIRGlobal&               lir_global,
	base::Optional<CRef<lir::Function>> global_ctor,
	base::Optional<CRef<lir::Function>> global_dtor
) {
	if (auto maybe_global = lir_global_to_dvm_data.atMaybe(&lir_global)) return **maybe_global;

	auto global_type = lowerAndKeepTslType(lir_global.layout);

	lir_global_to_dvm.put(
		&lir_global, DVMGlobal{ .name = lir_global.mangled_name, .type = global_type }
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

	lir_global_to_dvm_data.put(&lir_global, global_data);
	return lir_global_to_dvm_data.at(&lir_global);
}

const vm::code::Function& compiler::backend_vm::internal::ProgramLoweringContext::lowerAndKeepLirFunction(
	CRef<lir::Function> lir_function
) {
	if (auto maybe_lowered = lir_function_to_dvm.atMaybe(lir_function)) return **maybe_lowered;

	auto func_result_type = lowerAndKeepTslType(lir_function->return_type_layout);
	std::vector<vm::code::TypeOfData> func_param_types;
	for (const auto& param_layout: lir_function->parameter_layouts)
		func_param_types.push_back(lowerAndKeepTslType(param_layout));

	auto func_ctx = FunctionLoweringContext{ *this,
		                                     lir_function->mangled_name,
		                                     lir_function->return_type_layout,
		                                     lir_function->parameter_layouts };

	for (const auto& param: lir_function->local_list) {
		match_optional(param.parameter_index) {
			opt_some(_) func_ctx.registerFunctionParameter(&param);
			opt_none {
				// @TODO: #1656 Handle local variable inits properly.
				func_ctx.pushInit(&param);
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

vm::code::TypeOfData compiler::backend_vm::internal::ProgramLoweringContext::lowerTslTypeInternal(
	CRef<tsl::TypeLayout> layout
) {
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

			return vm::code::PrimitiveType(base::StrID(name.c_str()), bytes);
		}
		variant_case_novalue(tsl::FloatTypeLayout) {
			auto bits = usize(layout->getSize());
			CORE_ASSERT(
				bits == 16 || bits == 32 || bits == 64 || bits == 80, "Invalid size of float: ", bits
			);
			usize       bytes = bits / 8;
			std::string name  = "f" + std::to_string(bits);

			return vm::code::PrimitiveType(base::StrID(name.c_str()), bytes);
		}
		variant_default {
			CORE_PANIC(base::strConcat("Type not handled yet: ", layout->toStringIdentification()));
		}
	}
	CORE_UNREACHABLE();
}

std::expected<vm::code::CodeCollection, std::string> ProgramLoweringContext::validateAndProduceProgram() {
	auto collection        = vm::code::CodeCollection();
	collection.functions   = std::ranges::to<std::vector>(lir_function_to_dvm | std::views::values);
	collection.global_data = std::ranges::to<std::vector>(
		lir_global_to_dvm_data | std::views::values
		| std::views::transform([](const auto& tuple) { return tuple; })
	);
	collection.types = std::ranges::to<std::vector>(tsl_type_to_dvm | std::views::values);

	for (auto& type: collection.types) vm::code::serialize(type, std::cerr);
	for (auto& func: collection.functions) vm::code::serialize(func, std::cerr);

	try {
		auto valid = vm::code::ValidProgram::withBuiltins();
		valid      = valid.tryInsertCode(collection);
		return valid.produceValidCodeCollection();
	} catch (vm::code::ValidationError& e) { return std::unexpected(e.what()); }
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(compiler::backend_vm::internal::ProgramLoweringContext);
