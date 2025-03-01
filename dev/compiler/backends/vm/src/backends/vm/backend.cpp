#include "backend.hpp"
#include "base/variant.hpp"
#include "instructions.hpp"
#include <lir/lir_structure/lir_structure.hpp>
#include <ranges>

namespace compiler::backend_vm {

	void Module::buildRepr(std::ostream& out) const { file_builder.build().serialize(out); }

	void Module::addLirFunction(CRef<lir::Function> lir_function) {
		std::cerr << "Adding function: " << lir_function->name.strView() << "\n";
		auto&& local_layouts = lir_function->local_list
		                     | std::views::transform([](auto&& local) { return local->layout; });
		for (auto&& layout: local_layouts) {
			// @TODO
			// Transform layout to type:
			// file_builder.addType(const vm::TypeOfData &type)
		}

		auto block_to_id = lir_function->getBlockIDs();
		for (auto&& lir_block: lir_function->blocks) {
			// block->terminator
			auto         id = block_to_id.atMaybe(lir_block).expect("id of block not found");
			BlockBuilder block;
			std::string  label_str = base::strConcat(lir_function->name, "_label_", id);
			block.addInstruction(Op_label{ base::StrID(label_str.data()) });

			for (auto& instruction: lir_block->instructions) {
				// @TODO
				switch (instruction.operation) {
				case lir::Operation::Assign:
				case lir::Operation::IntegerAdd:
				case lir::Operation::IntegerSub:
				case lir::Operation::IntegerMul:
				case lir::Operation::IntegerUDiv:
				case lir::Operation::IntegerSDiv:
				case lir::Operation::IntegerUMod:
				case lir::Operation::IntegerSMod:
				case lir::Operation::IntegerULt:
				case lir::Operation::IntegerSLt:
				case lir::Operation::IntegerNeg:
				default:
					CORE_PANIC("Invalid operation: ", instruction.operation);
				}
			}

			// @TODO
			switch (lir_block->terminator.operation) {
			case lir::Operation::ReturnVoid:
			case lir::Operation::ReturnValue:
			case lir::Operation::Jump:
			case lir::Operation::Branch:

			case lir::Operation::COUNT:
			case lir::Operation::Uninitialized:
			default:
				CORE_PANIC("Invalid terminator: ", lir_block->terminator.operation);
			}
		}
	}

	Module::Module(base::StrID module_id): module_id(module_id) {}
}
