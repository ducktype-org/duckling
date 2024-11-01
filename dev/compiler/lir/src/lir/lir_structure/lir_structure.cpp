#include "lir_structure.hpp"

namespace compiler::lir {

	// @TODO: printing in this file in not perfect nor complete, make it better

	/**
	 * @brief This struct encapsulates the logic and shared state for printing LIR code.
	 * This state is needed because the LIR lacks any kind of "ids" or names for locals, blocks, etc
	 * The ids/names are given arbitrarily.
	 * 
	 * @note It should be used only used in lir::Function::debugPrint method
	 */
	struct LirPrinter {

	};

	void LirLocal::debugPrint(query::Context& ctx, std::ostream& output, bool detailed) const {
		// @TODO print some local identification
		output << "  Local(" << "???" << ")\n";
		if (detailed) output << "  TYPE:\n" << type.toStringDefinition(ctx, true, 1) << "\n";
	}

	void Function::debugPrint(query::Context& ctx, std::ostream& output) const {
		output << "Function \"" << name.strView() << "\" = {\n";
		output << " Locals:\n";

		for (const auto& local: local_list) {
			local->debugPrint(ctx, output, true);
			output << "\n";
		}
		
		size_t block_nr = 0;
		output << " Blocks:\n";
		for (auto block: block_order) {
			output << "  Block: " << block_nr << "\n";

			for (const auto& instruction: block->instructions) {
				output << "    ";
				instruction.debugPrint(output);
				output << "\n";
			}
			output << "    ";
			block->terminator.debugPrint(output);
			output << "\n";

			block_nr++;
		}

		output << "}\n";
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
