#include "builders.hpp"
#include "backends/vm/elements.hpp"
#include "backends/vm/instructions.hpp"
#include <base/ref.hpp>
#include <preprocessor/parser/types_of_data.hpp>
#include <ranges>

void compiler::backend_vm::BlockBuilder::addBlock(const BlockBuilder& block) {
	types.insert(types.end(), block.types.begin(), block.types.end());
	instructions.insert(instructions.end(), block.instructions.begin(), block.instructions.end());
}

compiler::backend_vm::Block compiler::backend_vm::BlockBuilder::build() const {
	auto block = Block();

	for (auto& type: types) block.instructions.emplace_back(Op_init_type{ type });
	block.instructions.insert(block.instructions.end(), instructions.begin(), instructions.end());
	for (auto& _: types | std::views::reverse) block.instructions.emplace_back(Op_deinit{});

	return block;
}

compiler::backend_vm::Function compiler::backend_vm::FunctionBuilder::build(
	const base::HashMap<base::StrID, CRef<vm::TypeOfData>>& available_types
) const {
	Function function;
	function.body = body.build();
	function.name = name;

	function.stack_size    = 1'337;
	function.arg_size      = 1'337;
	function.next_arg_size = 1'337;
	function.ret_size      = 1'337;

	return function;
}

void compiler::backend_vm::CodeFileBuilder::addFunction(const FunctionBuilder& function) {
	functions.push_back(function);
}

void compiler::backend_vm::CodeFileBuilder::addType(const vm::TypeOfData& type) {
	if (type_map.atMaybe(typeName(type))) {
		// Type already exists, check if it's the same and if true, skip
	} else {
		types.push_back(type);
		auto type_ref = base::CRef<vm::TypeOfData>(&types.back());
		type_map.put(typeName(type), type_ref);
	}
}

compiler::backend_vm::CodeFile compiler::backend_vm::CodeFileBuilder::build() const {
	auto file      = CodeFile();
	file.types     = types;
	file.functions = std::ranges::to<std::deque<Function>>(
		functions | std::views::transform([&](const auto& function_builder) {
			return function_builder.build(type_map);
		})
	);
	return file;
}

void compiler::backend_vm::BlockBuilder::addInstruction(const VmInstruction& instruction) {
	instructions.push_back(instruction);
}

compiler::backend_vm::FunctionBuilder::FunctionBuilder(base::StrID name): name(name) {}

void compiler::backend_vm::FunctionBuilder::addBlock(const BlockBuilder& block) {
	body.addBlock(block);
}
