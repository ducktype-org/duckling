#include "lir_structure.hpp"
#include <base/maps.hpp>

namespace compiler::lir {

	bool Function::validateBlockOrder() const {
		base::Map<BlockRef, bool> block_map;
		for (const auto& block: block_order) {
			if (block_map.contains(block)) return false;
			block_map.put(block, true);
		}
		for (const auto& block: blocks) {
			if (!block_map.contains(block.ref())) return false;
		}
		return true;
	}

	// @TODO: printing in this file in not perfect nor complete, make it better

	/**
	 * @brief This struct encapsulates the logic and shared state for printing LIR code.
	 * This state is needed because the LIR lacks any kind of "ids" or names for locals, blocks, etc
	 * The ids/names are given arbitrarily.
	 * 
	 * @note It should be used only used in lir::Function::debugPrint method
	 */
	struct LirPrinter {
		query::Context& ctx;
		std::ostream& output;

		LirPrinter(query::Context& ctx, std::ostream& output):
			ctx(ctx), output(output) {}

		base::Map<LocalRef, usize> local_id;
		base::Map<BlockRef, usize> block_id;

		void setLocalIds(const Function& function) {
			usize next_id = 0;
			for (const auto& local: function.local_list) {
				local_id[local.ref()] = next_id++;
			}
		}

		void setBlockIds(const Function& function) {
			usize next_id = 0;
			for (const auto& block: function.block_order) {
				block_id[block] = next_id++;
			}
		}

		void debugPrint(const Function& function) {
			setLocalIds(function);
		}

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
