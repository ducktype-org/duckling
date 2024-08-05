#include "mir_structure.hpp"
#include <base/variant.hpp>

namespace compiler::mir {


	void Function::debugPrint(std::ostream& output) const {
		output << "Function " << name.strView() << ": TODO -> TODO {\n";

		for (const auto& block : blocks) {
			output << "  block " << u64(block.id) << ":\n";
			for (const auto& instruction : block.instructions) {
				output << "    ";
				instruction.debugPrint(output);
				output << "\n";
			}
		}

		output << "}\n";
	}

	void Instruction::debugPrint(std::ostream& output) const {
		if (this->output.has_value()) {
			this->output.value()->debugPrint(output);
			output << " := ";
		} else {
			output << "    ";
		}

		output << base::enumToStr(operation).strView() << "  ";

		for (const auto& arg : arguments) {
			arg.debugPrint(output);
			output << ", ";
		}

		output << "\t[";
		for (const auto& flag : flags) {
			output << "Flag todo" << ", ";
		}
		output << "], scope:" << scope.customPerfectHash();
	}

	void MirLocal::debugPrint(std::ostream& output) const {
		output << "Local(" << u64(id) << ")";
	}

	void MirLocation::debugPrint(std::ostream& output) const {
		variant_match(this->value) {
			variant_case(LocalRef, local) {
				local->debugPrint(output);
			}
			variant_case(MirIntegerConst, value) {
				output << value.value;
			}
			variant_case(BlockID, block) {
				output << "Block(" << u64(block) << ")";
			}
			variant_default {
				RIFT_PANIC("Unexpected MirLocal alternative in mir debugPrint");
			}
		}	
	}
}

