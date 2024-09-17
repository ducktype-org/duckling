#include "mir_structure.hpp"
#include <base/variant.hpp>
#include <sstream>

namespace compiler::mir {

	bool isTerminating(Operation op) {
		switch (op) {
		case Operation::ReturnVoid:
		case Operation::ReturnValue:
		case Operation::Jump:
		case Operation::Branch:
		case Operation::FunctionEnd:
			return true;
		default:
			return false;
		}
	}

	void Function::debugPrint(std::ostream& output) const {
		output << "Function " << name.strView() << ": TODO -> TODO {\n";

		for (const auto& block: blocks | std::views::reverse) {
			output << "  block " << u64(block.id);
			if (block.id == entry_block) output << " [entry]";
			output << ":\n";
			for (const auto& instruction: block.instructions) {
				output << "    ";
				instruction.debugPrint(output);
				output << "\n";
			}
			output << "    ";
			block.terminator.debugPrint(output);
			output << "\n";
		}
		output << "}\n";
	}

	void Instruction::debugPrint(std::ostream& out) const {
		// save flags to restore
		auto out_flags = out.flags();

		if (output.has_value()) {
			output.value()->debugPrint(out);
			out << " := ";
		} else {
			out << "    ";
		}

		out << std::left << std::setw(15);
		out << base::enumToStr(operation).strView() << "  ";

		std::stringstream args;

		std::string_view separator = "";
		for (const auto& arg: arguments) {
			args << separator;
			arg.debugPrint(args);
			separator = ", ";
		}

		out << std::left << std::setw(15);
		out << args.str() << "  ";

		out << "Flags[";
		for ([[maybe_unused]] const auto& flag: flags) {
			out << "Flag todo"
				<< ", ";
		}
		out << "], scope:" << scope.customPerfectHash();

		// restore flags
		out.flags(out_flags);
	}

	void MirLocal::debugPrint(std::ostream& output) const { output << "Local(" << u64(id) << ")"; }

	void MirLocation::debugPrint(std::ostream& output) const {
		variant_match(this->value) {
			variant_case(LocalRef, local) { local->debugPrint(output); }
			variant_case(MirIntegerConst, value) { output << value.value; }
			variant_case(BlockID, block) { output << "Block(" << u64(block) << ")"; }
			variant_default { RIFT_PANIC("Unexpected MirLocal alternative in mir debugPrint"); }
		}
	}
}
