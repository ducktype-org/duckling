#include "mir_structure.hpp"
#include <base/variant.hpp>
#include <helios/symbols/symbols.hpp>
#include <sstream>

namespace compiler::mir {

	base::HashT Function::customPerfectHash() const { return base::perfectHash(helios_id); }

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
			return { terminator.arguments.at(0).get<BlockID>() };

		case Branch:
			return { terminator.arguments.at(1).get<BlockID>(),
				     terminator.arguments.at(2).get<BlockID>() };

		case ReturnVoid:
		case ReturnValue:
		case FunctionEnd:
			return {};

		default:
			CORE_PANIC("Not a terminator instruction");
		}
	}

	helios::ScopeID Block::beginScope() const {
		if (instructions.empty())
			return terminator.scope;
		else
			return instructions.at(0).scope;
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

	void Instruction::debugPrint(std::ostream& output_stream) const {
		// save flags to restore
		auto output_flags = output_stream.flags();

		output_stream << std::left << std::setw(12);
		std::stringstream output_value;
		if (this->output.has_value()) {
			this->output.value()->debugPrint(output_value);
			output_value << " :=";
		}
		output_stream << output_value.str() << " ";

		output_stream << std::left << std::setw(15);
		output_stream << base::enumToStr(operation).strView() << "  ";

		std::stringstream args;

		std::string_view separator = "";
		for (const auto& arg: arguments) {
			args << separator;
			arg.debugPrint(args);
			separator = ", ";
		}

		output_stream << std::left << std::setw(15);
		output_stream << args.str() << "  ";

		separator = "";
		output_stream << "Flags[";
		for ([[maybe_unused]]
		     const auto& flag: flags) {
			output_stream << separator;
			flag.debugPrint(output_stream);
			separator = ", ";
		}
		output_stream << "], scope:" << scope.customPerfectHash();

		// restore flags
		output_stream.flags(output_flags);
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
			variant_case(MirBoolConst, value) { output << (value.value ? "true" : "false"); }
			variant_case(BlockID, block) { output << "Block(" << u64(block) << ")"; }
			variant_default { CORE_PANIC("Unexpected MirLocal alternative in mir debugPrint"); }
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
