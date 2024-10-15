#include "lir_structure.hpp"

namespace compiler::lir {

	void Function::debugPrint(std::ostream& output) const {
		output << "Function: " << name.strView() << "\n";
		output << "Blocks:\n";
		// for (const auto& block : blocks) {
		// 	block->debugPrint(output);
		// }
	}
}
