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
				local_id.put(local.ref(), next_id);
				next_id++;
			}
		}

		void setBlockIds(const Function& function) {
			CORE_ASSERT(function.validateBlockOrder(), "Invalid block order");
			usize next_id = 0;
			for (const auto& block: function.block_order) {
				block_id.put(block, next_id);
				next_id++;
			}
		}

		void printLocalDesc(LocalRef local) {
			output << "  Local(" << local_id[local] << ")\n";
			output << "    TYPE:\n" << local->type.toStringDefinition(ctx, true, 1) << "\n";
		}

		void printLocal(LocalRef local) {
			output << "Local(" << local_id[local] << ")";
		}

		void printLocation(const LirLocation& location) {
			variant_match(location.getVariant()) {
				variant_case(i64, value) {
					output << value;
				}
				variant_case(LocalRef, local) {
					printLocal(local);
				}
				variant_case(BlockRef, block) {
					output << "Block(" << block_id[block] << ")";
				}
				variant_default {
					CORE_PANIC("Unhandled variant in printLocation");
				}
			}
		}

		void printInstruction(const Instruction& instruction) {
			if (instruction.output.has_value()) {
				printLocal(instruction.output.value());
				output << " := ";
			}
			else {
				output << "            ";
			}
			output << base::enumToStr(instruction.operation).strView() << "  ";

			std::string_view sep = "";
			for (auto arg: instruction.arguments) {
				output << sep;
				sep = ", ";
				printLocation(arg);
			}
		}

		void debugPrint(const Function& function) {
			setLocalIds(function);
			setBlockIds(function);

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
