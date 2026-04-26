#include "jit_data.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <memory>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/Module.h>

LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> setupModule(const std::string& module_name, llvm::LLVMContext& ctx) {
	auto&             llvm_data     = llvmData();
	Ref<llvm::Module> master_module = llvm_data.g_module.get();
	auto              new_mod       = std::make_unique<llvm::Module>(module_name, ctx);
	new_mod->setDataLayout(master_module->getDataLayout());
	new_mod->setTargetTriple(master_module->getTargetTriple());
	return new_mod;
}

/**
 * @brief Constructs an array of non-executable opcodes (like ext_*).
 */
constexpr std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()> constructNonExecOpcodeArray(
) {
	auto non_executable_opcodes
		= vm::low::OPCODE_NAMES | std::views::enumerate
	    | std::views::filter([](auto pair) { return std::get<1>(pair).starts_with("ext_"); })
	    | std::views::transform([](auto pair) {
			  return static_cast<vm::low::MicroOpcode>(std::get<0>(pair));
		  });

	std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()> output{};

	std::ranges::copy(non_executable_opcodes, output.begin());

	return output;
}

static constexpr std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()>
	NON_EXEC_OPCODES = constructNonExecOpcodeArray();

bool isOpcodeNonExecutable(const vm::low::MicroOpcode& opcode) {
	return std::find(NON_EXEC_OPCODES.begin(), NON_EXEC_OPCODES.end(), opcode) != NON_EXEC_OPCODES.end();
}