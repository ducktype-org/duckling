#include "function_lowering_context.hpp"

#include "dvm_value.hpp"
#include "lir/lir_structure/lir_structure.hpp"
#include "program_lowering_context.hpp"

using namespace compiler::backend_vm::internal;

#define INVALID_CASE(tp, reason)                                                    \
	variant_case(tp, _) {                                                           \
		CORE_PANIC("During handling of type ", base::typeName<tp>(), ": ", reason); \
	}

#define NOIMPL_CASE(tp, reason)                                              \
	variant_case(tp, _) {                                                    \
		throw base::NotYetImplemented(                                       \
			base::strConcat("Type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                   \
	}

FunctionLoweringContext::FunctionLoweringContext(
	ProgramLoweringContext& program_context, CRef<lir::Function> lir_function
):
	  program_context(program_context),
	  lir_function(lir_function) {
	insertParameterLocals();
}

base::StrID FunctionLoweringContext::getBlockLabel(lir::BlockRef block) {
	if (!block_to_label.contains(block)) {
		auto label_name = base::strConcat("label_", block_to_label.size());
		block_to_label.put(block, base::StrID(label_name.data()));
	}

	return block_to_label.at(block);
}

DVMValue FunctionLoweringContext::lowerLirValue(const lir::LIRValue& lir_value) {
	// i64, bool, LIRPlace, BlockRef, FunctionLiteral
	variant_match(lir_value.getVariant()) {
		variant_case(i64, value) { return { DVMImmediate(value) }; }
		variant_case(bool, value) { return { DVMImmediate(value) }; }
		variant_case(lir::LIRPlace, place) {
			// @TODO: #1560 handle access into fields.
			variant_match(place.base) {
				variant_case(lir::LIRLocalRef, local_ref) { return { getDVMLocal(local_ref) }; }
				variant_case(lir::LIRGlobal, global) {
					return { program_context.getDVMGlobal(global) };
				}
			}
		}
		variant_case(lir::BlockRef, block_ref) { return { DVMLabel{ getBlockLabel(block_ref) } }; }
		variant_case(lir::FunctionLiteral, function) {
			return { DVMFunctionName{ function.mangled_name } };
		}
		variant_default { CORE_PANIC("Unhandled value case"); }
	}
	CORE_UNREACHABLE();
}

const DVMLocal& FunctionLoweringContext::getDVMLocal(lir::LIRLocalRef local) {
	if (!lir_local_to_dvm.contains(local)) {
		auto new_local = insertLocalToDVMMapping(local);
		// @TODO: initType(new_local.name, new_local.type);
	}
	return lir_local_to_dvm.at(local);
}

void FunctionLoweringContext::insertParameterLocals() {
	for (const auto& local: lir_function->local_list) {
		if_opt_some(local.parameter_index, _) { insertLocalToDVMMapping(&local); }
	}
}

const DVMLocal& FunctionLoweringContext::insertLocalToDVMMapping(lir::LIRLocalRef lir_local) {
	auto var_type = program_context.lowerTslType(lir_local->layout);
	auto var_name = base::StrID(base::strConcat("var", lir_local_to_dvm.size()).c_str());
	lir_local_to_dvm.put(
		lir_local, DVMLocal{ .name = base::StrID(var_name.data()), .type = var_type }
	);
	return lir_local_to_dvm.at(lir_local);
}
