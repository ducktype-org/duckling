#include "lir_structure.hpp"

namespace compiler::lir {

	void LirLocal::debugPrint(std::ostream& output, bool detailed) const {
		// some id? lol
		// @TODO...
		output << "Local("
			   << "???"
			   << ")";
		if (detailed) output << " type: ???";
	}

	void Function::debugPrint(std::ostream& output) const {
		output << "Function: " << name.strView() << "\n";
		output << "Locals:\n";

		for (const auto& local: local_list) {
			local->debugPrint(output, true);
			output << "\n";
		}

		output << "Blocks:\n";
		for (auto block: block_order) {
			output << "Block: "
				   << "???"
				   << "\n";
			for (const auto& instruction: block->instructions) {
				output << "    ";
				instruction.debugPrint(output);
				output << "\n";
			}
			output << "    ";
			block->terminator.debugPrint(output);
			output << "\n";
		}
	}

	void Instruction::debugPrint(std::ostream& output) const {
		if (this->output.has_value()) {
			// this->output.value()->debugPrint(output);
			output << " :=";
		}
		output << " ";
		output << base::enumToStr(operation).strView() << "  ";

		// some args...
	}
}
