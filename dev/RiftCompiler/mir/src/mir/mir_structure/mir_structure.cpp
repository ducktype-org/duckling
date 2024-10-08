#include "mir_structure.hpp"
#include <base/variant.hpp>
#include <helios/symbols/symbols.hpp>
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

	std::vector<BlockID> getTerminatorSuccessors(const Instruction& terminator) {
		using enum Operation;
		switch (terminator.operation) {
		case Jump:
			return { terminator.arguments.at(0).asT<BlockID>() };
			break;

		case Branch:
			return { terminator.arguments.at(1).asT<BlockID>(),
				     terminator.arguments.at(2).asT<BlockID>() };
			break;

		case ReturnVoid:
			[[fallthrough]];
		case ReturnValue:
			[[fallthrough]];
		case FunctionEnd:
			return {};

		default:
			RIFT_PANIC("Not a terminator instruction");
		}
	}

	void Function::debugPrint(std::ostream& output) const {
		output << "Function " << name.strView() << ": TODO -> TODO\n";

		for (auto& local: this->local_list) {
			output << "    ";
			local->debugPrint(output, true);
			output << "\n";
		}
		output << "{\n";

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

	void Instruction::debugPrint(std::ostream& output) const {
		// save flags to restore
		auto output_flags = output.flags();

		output << std::left << std::setw(12);
		std::stringstream output_value;
		if (this->output.has_value()) {
			this->output.value()->debugPrint(output_value);
			output_value << " :=";
		}
		output << output_value.str() << " ";

		output << std::left << std::setw(15);
		output << base::enumToStr(operation).strView() << "  ";

		std::stringstream args;

		std::string_view separator = "";
		for (const auto& arg: arguments) {
			args << separator;
			arg.debugPrint(args);
			separator = ", ";
		}

		output << std::left << std::setw(15);
		output << args.str() << "  ";

		separator = "";
		output << "Flags[";
		for ([[maybe_unused]] const auto& flag: flags) {
			output << separator;
			flag.debugPrint(output);
			separator = ", ";
		}
		output << "], scope:" << scope.customPerfectHash();

		// restore flags
		output.flags(output_flags);
	}

	void MirLocal::debugPrint(std::ostream& output, bool detailed) const {
		output << "Local(" << u64(id) << ")";
		if (detailed) {
			output << ": Helios Name: " << name(this->helios_id).strView();
			output << ", Type: ";
			output << this->type.getType().toString();
			output << ", Lifetime Scope: " << this->lifetime_scope.customPerfectHash();
		}
	}

	base::StrID MirLocal::getName() const { return name(this->helios_id); }

	void MirLocation::debugPrint(std::ostream& output) const {
		variant_match(this->value) {
			variant_case(LocalRef, local) { local->debugPrint(output); }
			variant_case(MirIntegerConst, value) { output << value.value; }
			variant_case(BlockID, block) { output << "Block(" << u64(block) << ")"; }
			variant_default { RIFT_PANIC("Unexpected MirLocal alternative in mir debugPrint"); }
		}
	}

	void OperationFlag::debugPrint(std::ostream& output) const {
		switch (flag) {
		case Flag::Construct:
			output << "Construct";
			break;
		case Flag::Destruct:
			output << "Destruct";
			break;
		case Flag::Move:
			output << "Move";
			break;
		}
		output << " ";
		local->debugPrint(output);
	}
}
