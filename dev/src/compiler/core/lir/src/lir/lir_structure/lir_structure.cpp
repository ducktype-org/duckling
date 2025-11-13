#include "lir_structure.hpp"

#include <helios/symbols/simple.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <iomanip>
#include <set>
#include <variant>

namespace compiler::lir {

	base::Map<BlockRef, u64> Function::getBlockIDs() const {
		CORE_ASSERT(this->validateBlockOrder().isOk(), "Invalid block order");

		base::Map<BlockRef, usize> block_ids;
		usize                      next_id = 0;
		for (const auto& block: block_order) {
			block_ids.put(block, next_id);
			next_id++;
		}
		return block_ids;
	}

	base::Map<LIRLocalRef, u64> Function::getLocalVariableIDs() const {
		base::Map<LIRLocalRef, usize> local_ids;
		usize                         next_id = 0;
		for (const auto& local: local_list) {
			local_ids.put(&local, next_id);
			next_id++;
		}
		return local_ids;
	}

	base::OkBad Function::validateBlockOrder() const {
		base::Map<BlockRef, bool> block_map;
		for (const auto& block: block_order) {
			if (block_map.contains(block)) return base::BAD;
			block_map.put(block, true);
		}
		for (const auto& block: blocks)
			if (!block_map.contains(&block)) return base::BAD;
		return base::OK;
	}

	base::OkBad Function::validateParameters() const {
		std::set<u64> parameter_indexes;

		for (const auto& local: local_list) {
			if (local.parameter_index.has_value()) {
				auto index = local.parameter_index.value();
				if (parameter_indexes.contains(index)) return base::BAD;

				parameter_indexes.insert(index);
				if (index >= parameter_layouts.size()) return base::BAD;
				if (local.layout != parameter_layouts.at(index)) return base::BAD;
			}
		}

		if (parameter_layouts.size() != parameter_indexes.size()) return base::BAD;

		return base::OK;
	}

	/**
	 * @brief This struct encapsulates the logic and shared state for printing LIR code.
	 * This state is needed because the LIR lacks any kind of "ids" or names for locals, blocks, etc
	 * The ids/names are given arbitrarily.
	 *
	 * @note It should be used only used in lir::Function::debugPrint method
	 */
	struct LIRPrinter {
		query::Context& ctx;
		std::ostream&   output;

		base::Map<LIRLocalRef, usize> local_id;
		base::Map<BlockRef, usize>    block_id;

		LIRPrinter(query::Context& ctx, std::ostream& output): ctx(ctx), output(output) {}

		void printLocalDesc(LIRLocalRef local) {
			output << "  Local(" << local_id[local] << ")";
			if (local->helios_id.has_value())
				output << ", helios_name: " << name(local->helios_id.value()).strView();
			if (local->parameter_index.has_value())
				output << ", parameter_index: " << local->parameter_index.value();
			output << "\n";
			output << "    LAYOUT:\n" << local->layout->toStringDefinition(ctx, true, 1) << "\n";
		}

		/**
		 * @note Custom output, so we can align when printing instruction
		 */
		void printLocal(LIRLocalRef local, std::ostream& loc_output) const {
			loc_output << "Local(" << local_id[local] << ")";
		}

		void printGlobal(const LIRGlobal& global, std::ostream& loc_output) const {
			loc_output << "Global(" << global.mangled_name.strView() << ")";
		}

		void printOutput(const LIRPlace& output, std::ostream& loc_output) {
			variant_match(output.base) {
				variant_case(LIRLocalRef, local) { printLocal(local, loc_output); }
				variant_case(LIRGlobal, global) { printGlobal(global, loc_output); }
			}
			for (const auto& arg: output.access_chain) loc_output << "." << name(arg).strView();
		}

		void printValue(const LIRValue& location) {
			variant_match(location.getVariant()) {
				variant_case(i64, value) { output << value; }
				variant_case(bool, value) { output << (value ? "true" : "false"); }
				variant_case(LIRPlace, place) {
					variant_match(place.base) {
						variant_case(LIRLocalRef, local) { printLocal(local, output); }
						variant_case(LIRGlobal, global) { printGlobal(global, output); }
					}
					for (const auto& arg: place.access_chain) output << "." << name(arg).strView();
				}
				variant_case(BlockRef, block) { output << "Block(" << block_id[block] << ")"; }
				variant_case(FunctionLiteral, func) {
					output << "Func(" << func.mangled_name.strView() << ")";
				}
				variant_default { CORE_PANIC("Unhandled variant in printValue"); }
			}
		}

		void printInstruction(const Instruction& instruction) {
			// save flags to restore
			auto output_flags = output.flags();

			output << std::left << std::setw(12);
			std::stringstream output_value;
			if (instruction.output.has_value()) {
				printOutput(instruction.output.value(), output_value);
				output_value << " :=";
			}
			output << output_value.str() << " ";

			output << std::left << std::setw(15);
			output << base::enumToStr(instruction.operation).strView() << "  ";

			std::string_view sep = "";
			for (auto arg: instruction.arguments) {
				output << sep;
				sep = ", ";
				printValue(arg);
			}

			// restore flags
			output.flags(output_flags);
		}

		void debugPrint(const Function& function) {
			// set local and block ids:
			local_id = function.getLocalVariableIDs();
			block_id = function.getBlockIDs();

			output << "[LIR] Function \"" << function.mangled_name.strView() << "\":\n";

			for (const auto& local: function.local_list) {
				printLocalDesc(&local);
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
		LIRPrinter{ ctx, output }.debugPrint(*this);
	}

	FunctionLiteral FunctionLiteral::fromFunction(const Function& function) {
		return FunctionLiteral{
			.mangled_name = function.mangled_name,
			.abi          = function.abi,
			.parameter_layouts
			= std::make_shared<std::vector<CRef<tsl::TypeLayout>>>(function.parameter_layouts),
			.return_type_layout = function.return_type_layout
		};
	}
}
