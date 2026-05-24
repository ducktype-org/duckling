#include "fast_compiler.hpp"

#include "type_builder.hpp"

#include "base/except/exceptions.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/loader/compiler/compiler.hpp>
#include <vm/loader/compiler/fast/instruction_lowering.hpp>

#include <algorithm>

using namespace vm::code;
using namespace vm::loader;
using namespace vm::loader::compiler;

compiler::fast::FastCompiler::FastCompiler(const ValidProgram& high_program):
	  Compiler(high_program) {}

CRef<vm::fast::ProgramBase> vm::loader::compiler::fast::FastCompiler::getProgramBase() const {
	return &program;
}

CRef<vm::fast::reloc::RelocFunctionCollection> vm::loader::compiler::fast::FastCompiler::
	getRelocatableFunctions() const {
	return &reloc_functions;
}

void fast::FastCompiler::compileNewTypes(const std::vector<valid_type::ValidType>& new_types) {
	if (std::ranges::empty(new_types)) return;

	rebuildFastTypeCollection(&program.types, high_program.getTypeContext().getCurrentTypes());
}

void fast::FastCompiler::compileNewGlobals(const std::vector<GlobalData>& new_globals) {
	for (const auto& global: new_globals) {
		program.global_data.insert(
			vm::fast::GlobalData{
				.name                 = global.name,
				.type                 = program.types.at(global.type),
				.global_buffer_offset = program.global_buffer_size.asInt(),
			},
			global.name
		);

		program.global_buffer_size += program.types.at(global.type)->getSize();
	}
}

void fast::FastCompiler::compileNewFunctions(const std::vector<Function>& new_functions) {
	for (const Function& function: new_functions) {
		detail::FunctionStackContext ctx = calculateStackContext(function);
		program.functions.insert(
			vm::fast::FunctionInfo{
				.name        = function.name,
				.id          = vm::fast::FunctionID(program.functions.size()),
				.return_size = std::ranges::fold_left(
					function.signature.result_types
						| std::views::transform([this](const auto& result_type) {
							  return program.types.at(result_type)->getSize();
						  }),
					Bytes(0),
					std::plus()
				),
				.args_size = std::ranges::fold_left(
					function.signature.parameters
						| std::views::transform([this](const auto& param_type) {
							  return program.types.at(param_type)->getSize();
						  }),
					Bytes(0),
					std::plus()
				),
				.arg_types    = function.signature.parameters
		                      | std::views::transform([this](const auto& param_type) {
								 return program.types.at(param_type)->getID();
								})
		                      | std::ranges::to<std::vector<vm::fast::TypeID>>(),
				.return_types = function.signature.result_types
		                      | std::views::transform([this](const auto& result_type) {
									return program.types.at(result_type)->getID();
								})
		                      | std::ranges::to<std::vector<vm::fast::TypeID>>() },
			function.name
		);
		reloc_functions.emplace_back(
			lowerInstructions(high_program, program, ctx, *program.functions.at(function.name))
		);
	}
}

void fast::FastCompiler::compileNewExtCFunctions(
	const std::vector<code::ExternalCFunction>& new_functions
) {
	for (const ExternalCFunction& func: new_functions) {
		base::Optional<vm::fast::TypeCRef> result_type = std::nullopt;
		if (func.signature.result_types.size() == 1)
			result_type = program.types.at(func.signature.result_types[0]);
		else if (func.signature.result_types.size() > 1)
			CORE_PANIC("External C function cannot have more than 1 return type");

		program.extern_c_functions.insert(
			vm::fast::ExternCFunction{
				.name               = func.name,
				.function_pointer   = func.function_pointer,
				.parameter_size_sum = std::ranges::fold_left(
					func.signature.parameters | std::views::transform([this](const auto& param_name) {
						return Bytes(program.types.at(param_name)->getSize().asInt());
					}),
					Bytes(0),
					std::plus()
				),
				.parameter_types = func.signature.parameters
		                         | std::views::transform([this](const auto& param_name) {
									   return program.types.at(param_name);
								   })
		                         | std::ranges::to<std::vector<vm::fast::TypeCRef>>(),
				.result_type     = result_type

			},
			func.name
		);
	}
}

ProgramSize fast::FastCompiler::getCurrentProgramSize() const {
	return ProgramSize{
		.function_count       = program.functions.size(),
		.global_count         = program.global_data.size(),
		.type_count           = program.types.size(),
		.ext_c_function_count = program.extern_c_functions.size(),
	};
}
