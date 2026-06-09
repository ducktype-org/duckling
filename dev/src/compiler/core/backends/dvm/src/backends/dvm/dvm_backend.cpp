#include <backends/dvm/dvm_backend.hpp>
#include <program_lowering_context.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <logger/logger.hpp>

namespace compiler::backend_vm {

	DVMCodeBuilder::DVMCodeBuilder(
		query::Context& query_ctx, bool build_debug_info, bool is_comp_time_lowering
	):
		  program_context(makeBox<internal::ProgramLoweringContext>(
			  query_ctx, build_debug_info, is_comp_time_lowering
		  )),
		  build_debug_info(build_debug_info) {}

	vm::code::CodeCollection DVMCodeBuilder::build() const {
		return program_context->produceCodeCollection();
	}

	base::Optional<debug_info::DebugInfo> DVMCodeBuilder::buildDebugInfo() {
		if (build_debug_info) return program_context->buildDebugInfo();
		return {};
	}

	void DVMCodeBuilder::insertLIRUnit(const lir::LIRUnit& lir_unit) {
		auto lir_function_deals_with_strings = [](const CRef<lir::Function> lir_function) {
			auto is_string_layout = [](const CRef<tsl::TypeLayout> layout) -> bool {
				return layout->getSourceType().getType().getKind() == tsh::Kind::String;
			};
			if (is_string_layout(lir_function->return_type_layout)) return true;
			for (const auto& param_layout: lir_function->parameter_layouts)
				if (is_string_layout(param_layout)) return true;
			return false;
		};

		for (const auto& lir_global: lir_unit.lir_globals) insertLIRGlobal(lir_global);
		for (const auto& lir_function: lir_unit.lir_functions) {
			// @TODO: #2483 Remove this filter (and the helper function) when strings work in DVM.
			if (lir_function_deals_with_strings(lir_function)) {
				CORE_DEV_LOG(
					Backend,
					"Lowering function that deals with strings in DVM backend skipped: ",
					lir_function->mangled_name,
					"\n"
				);
				continue;
			}

			insertLIRFunction(lir_function);
		}
	}

	void DVMCodeBuilder::insertLIRFunction(CRef<lir::Function> lir_function) {
		program_context->lowerAndKeepLirFunction(lir_function);
	}

	void DVMCodeBuilder::insertExternCFunction(const vm::code::ExternalCFunction& extern_func) {
		program_context->insertExternCFunction(extern_func);
	}

	void DVMCodeBuilder::insertLIRGlobal(const lir::LIRGlobalData& lir_global) {
		program_context->lowerAndKeepLirGlobal(lir_global);
	}

	void DVMCodeBuilder::insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode) {
		program_context->insertRawBytecodeDefinitions(bytecode);
	}
}
