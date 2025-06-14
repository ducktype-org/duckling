#include "mir_structure.hpp"

#include <helios/symbols/simple.hpp>

#include <base/variant.hpp>

#include <iomanip>
#include <sstream>

namespace compiler::mir {
	Function::Function(
		base::StrID                         name,
		tsh::SymbolType<>                   return_type,
		std::vector<tsh::SymbolType<>>      parameter_types,
		base::StableHashMap<BlockID, Block> blocks,
		std::vector<BlockID>                block_order,
		base::StableVector<const MirLocal>  local_list,
		LifetimeScopeTree                   lifetime_scope_tree,
		ScopeRef                            no_lifetime_scope,
		helios::SymID                       helios_id
	):
		  name(name),
		  return_type(return_type),
		  parameter_types(std::move(parameter_types)),
		  blocks(std::move(blocks)),
		  block_order(std::move(block_order)),
		  local_list(std::move(local_list)),
		  lifetime_scope_tree(std::move(lifetime_scope_tree)),
		  no_lifetime_scope(no_lifetime_scope),
		  helios_id(helios_id) {}

	u64 Function::queryUnstablePerfectHash() const { return helios_id.queryUnstablePerfectHash(); }

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

	ScopeRef Block::beginScope() const {
		if (instructions.empty())
			return terminator.scope;
		else
			return instructions.at(0).scope;
	}

	void Function::debugPrint(std::ostream& output) const {
		output << "[MIR] Function " << name.strView() << ": ";
		output << "(";
		std::string_view separator = "";
		for (auto& type: this->parameter_types) {
			output << separator << type.toString();
			separator = ", ";
		}
		output << ")";
		output << " -> "
			   << this->return_type.toString() << "\n";

		for (auto& local: this->local_list) {
			output << "    ";
			local->debugPrint(output, true);
			output << "\n";
		}
		output << "No Lifetime Scope: " << no_lifetime_scope->id << "\n";
		output << "{\n";

		for (const auto block_id: block_order) {
			const auto& block = blocks[block_id];

			output << "  block " << u64(block.id);
			if (block.id == block_order[0]) output << " [entry]";
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
		output << "], scope:" << scope->id;

		// restore flags
		output.flags(output_flags);
	}

	void MirLocal::debugPrint(std::ostream& output, bool detailed) const {
		output << "Local(" << u64(id) << ")";
		if (detailed) {
			output << ": Helios Name: " << getName().strView();
			output << ", Type: ";
			output << this->type.toString();
			output << ", Lifetime Scope: " << this->scope.value()->id;
			if (parameter_index.has_value())
				output << ", Parameter Index: " << parameter_index.value();
		}
	}

	base::StrID MirLocal::getName() const {
		if (helios_id.has_value()) return name(helios_id.value());
		return base::StrID(base::strConcat(id.asInt(), ".tmp").c_str());
	}

	void MirLocal::setLifetimeScope(ScopeRef scope) {
		CORE_ASSERT(this->scope.empty(), "lifetime_scope is already set");
		this->scope.emplace(scope);
	}

	void MIRValue::debugPrint(std::ostream& output) const {
		variant_match(this->value) {
			variant_case(LocalRef, local) { local->debugPrint(output); }
			variant_case(MirIntegerConst, value) { output << value.value; }
			variant_case(MirBoolConst, value) { output << (value.value ? "true" : "false"); }
			variant_case(BlockID, block) { output << "Block(" << u64(block) << ")"; }
			variant_case(MirFunctionLiteral, func) {
				output << "Function(" << name(func.helios_id).strView() << ")";
			}
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

	base::OkBad Function::validateBlockIDs() const {
		if (blocks.size() != block_order.size()) return base::BAD;

		for (const auto& block_id: block_order) {
			if (blocks.atMaybe(block_id).empty()) return base::BAD;
			if (blocks[block_id].id != block_id) return base::BAD;
		}

		for (const auto& block_id: block_order) {
			const auto& block = blocks[block_id];

			auto successors = getTerminatorSuccessors(block.terminator);
			for (const auto successor: successors)
				if (not blocks.contains(successor)) return base::BAD;
		}

		return base::OK;
	}
}
