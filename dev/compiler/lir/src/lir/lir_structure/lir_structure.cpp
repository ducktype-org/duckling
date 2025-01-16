#include "lir_structure.hpp"
#include <base/maps.hpp>

namespace compiler::lir {

	base::Map<BlockRef, u64> Function::getBlockIDs() const {
		CORE_ASSERT(this->validateBlockOrder(), "Invalid block order");

		base::Map<BlockRef, usize> block_ids;
		usize                      next_id = 0;
		for (const auto& block: block_order) {
			block_ids.put(block, next_id);
			next_id++;
		}
		return block_ids;
	}

	base::Map<LocalRef, u64> Function::getLocalVariableIDs() const {
		base::Map<LocalRef, usize> local_ids;
		usize                      next_id = 0;
		for (const auto& local: local_list) {
			local_ids.put(local.ref(), next_id);
			next_id++;
		}
		return local_ids;
	}

	bool Function::validateBlockOrder() const {
		base::Map<BlockRef, bool> block_map;
		for (const auto& block: block_order) {
			if (block_map.contains(block)) return false;
			block_map.put(block, true);
		}
		for (const auto& block: blocks)
			if (!block_map.contains(block.ref())) return false;
		return true;
	}

	/**
	 * @brief This struct encapsulates the logic and shared state for printing LIR code.
	 * This state is needed because the LIR lacks any kind of "ids" or names for locals, blocks, etc
	 * The ids/names are given arbitrarily.
	 *
	 * @note It should be used only used in lir::Function::debugPrint method
	 */
	struct LirPrinter {
		query::Context& ctx;
		std::ostream&   output;

		base::Map<LocalRef, usize> local_id;
		base::Map<BlockRef, usize> block_id;

		LirPrinter(query::Context& ctx, std::ostream& output): ctx(ctx), output(output) {}

		void printLocalDesc(LocalRef local) {
			output << "  Local(" << local_id[local] << ")";
			if (local->helios_id.has_value())
				output << ", helios_name: " << name(local->helios_id.value()).strView();
			output << "\n";
			output << "    LAYOUT:\n" << local->layout.toStringDefinition(ctx, true, 1) << "\n";
		}

		/**
		 * @note Custom output, so we can align when printing instruction
		 */
		void printLocal(LocalRef local, std::ostream& loc_output) const {
			loc_output << "Local(" << local_id[local] << ")";
		}

		void printLocation(const LirLocation& location) {
			variant_match(location.getVariant()) {
				variant_case(i64, value) { output << value; }
				variant_case(bool, value) { output << (value ? "true" : "false"); }
				variant_case(LocalRef, local) { printLocal(local, output); }
				variant_case(BlockRef, block) { output << "Block(" << block_id[block] << ")"; }
				variant_default { CORE_PANIC("Unhandled variant in printLocation"); }
			}
		}

		void printInstruction(const Instruction& instruction) {
			// save flags to restore
			auto output_flags = output.flags();

			output << std::left << std::setw(12);
			std::stringstream output_value;
			if (instruction.output.has_value()) {
				printLocal(instruction.output.value(), output_value);
				output_value << " :=";
			}
			output << output_value.str() << " ";

			output << std::left << std::setw(15);
			output << base::enumToStr(instruction.operation).strView() << "  ";

			std::string_view sep = "";
			for (auto arg: instruction.arguments) {
				output << sep;
				sep = ", ";
				printLocation(arg);
			}

			// restore flags
			output.flags(output_flags);
		}

		void debugPrint(const Function& function) {
			// set local and block ids:
			local_id = function.getLocalVariableIDs();
			block_id = function.getBlockIDs();

			output << "Function \"" << function.name.strView() << "\":\n";

			for (const auto& local: function.local_list) {
				printLocalDesc(local.ref());
				output << "\n";
			}
			output << "{\n";

			for (auto block: function.block_order) {
				output << "  Block " << block_id[block] << ":\n";

				for (const auto& instruction: block->instructions) {
					output << "    ";
					printInstruction(instruction);
					output << "\n";
				}
				output << "    ";
				printInstruction(block->terminator);
				output << "\n";
			}

			output << "}";
		}
	};

	void Function::debugPrint(query::Context& ctx, std::ostream& output) const {
		LirPrinter{ ctx, output }.debugPrint(*this);
	}
}
