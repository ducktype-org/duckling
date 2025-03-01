#include "builders.hpp"

void compiler::backend_vm::BlockBuilder::addBlock(const BlockBuilder& block) {
	instructions.insert(instructions.end(), block.instructions.begin(), block.instructions.end());
}

compiler::backend_vm::Block compiler::backend_vm::BlockBuilder::build() const {
	throw base::NotYetImplemented("BlockBuilder::build");
}

compiler::backend_vm::Function compiler::backend_vm::FunctionBuilder::build() const {
	throw base::NotYetImplemented("FunctionBuilder::build");
}

void compiler::backend_vm::CodeFileBuilder::addFunction(const FunctionBuilder& function) {
	functions.push_back(function.build());
}

void compiler::backend_vm::CodeFileBuilder::addType(const vm::TypeOfData& type) {
	types.push_back(type);
}

compiler::backend_vm::CodeFile compiler::backend_vm::CodeFileBuilder::build() const {
	throw base::NotYetImplemented("FileBuilder::build");
}

void compiler::backend_vm::BlockBuilder::addInstruction(const VmInstruction& instruction) {
	instructions.push_back(instruction);
}
