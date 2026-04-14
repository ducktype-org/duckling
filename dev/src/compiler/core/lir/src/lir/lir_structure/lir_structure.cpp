#include "lir_structure.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <tsl/queries.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <iomanip>
#include <set>

namespace compiler::lir {
	LIRLocal LIRLocal::fromMIR(
		query::Context&           ctx,
		const mir::MIRLocalRef    mir_local,
		const base::Optional<u64> new_parameter_index
	) {
		auto             type_layout = ctx.query<tsl::QuerySymbolTypeLayout>(mir_local->type);
		LIRLocalMetadata metadata;
		if_opt_some(mir_local->helios_id, helios_id) {
			metadata.source_code_name = helios::name(helios_id);
			if_opt_some(helios::symbolPst(helios_id), pst_elem) {
				metadata.position = pst_elem.unlock(ctx)->getHashCodePosition();
			}
		}
		return LIRLocal{ mir_local->helios_id, type_layout, new_parameter_index, metadata };
	}

	LIRLocal LIRLocal::boolLocal(query::Context& ctx) {
		auto bool_type   = tsh::getBoolType();
		auto bool_layout = ctx.query<tsl::QueryAbstractTypeLayout>(bool_type);

		return LIRLocal{ bool_layout };
	}

	LIRGlobal LIRGlobal::fromMIR(query::Context& ctx, mir::MIRGlobal mir_global) {
		auto type_layout = ctx.query<tsl::QuerySymbolTypeLayout>(mir_global.type);

		auto mangled_name = helios::mangler::getSimpleMangledName(ctx, mir_global.helios_id);

		return LIRGlobal{ mir_global.helios_id, type_layout, mangled_name };
	}

	LIRGlobal LIRGlobal::fromHOUT(query::Context& ctx, const helios::HOUTGlobalData& hout_global) {
		auto type_layout = ctx.query<tsl::QuerySymbolTypeLayout>(hout_global.type);

		auto mangled_name = helios::mangler::getSimpleMangledName(ctx, hout_global.helios_symbol);

		variant_match(hout_global.value) {
			variant_case(helios::HOUTGlobalConst, name) {
				return LIRGlobal{
					hout_global.helios_symbol, type_layout, mangled_name,
					LIRGlobalType::Constant,   name.value,
				};
			}
			variant_case(helios::HOUTGlobalVariable, name) {
				return LIRGlobal{
					hout_global.helios_symbol, type_layout, mangled_name, LIRGlobalType::Variable
				};
			}
			variant_default {
				CORE_PANIC(
					"Unhandled HOUTGlobalData type in LIRGlobal::fromHOUT: ",
					hout_global.original_name.strView()
				);
			}
		}

		CORE_UNREACHABLE();
	}

	LIRPlace::LIRPlace(BaseVariant base, std::vector<Projection> projection_chain):
		  base(std::move(base)),
		  layout([&]() -> CRef<tsl::TypeLayout> {
			  // Calculate the end layout of LIRPlace. Start with the root layout and go through the
		      // projections.
			  CRef<tsl::TypeLayout> current_layout = getBaseLayout();

			  for (const auto& proj: projection_chain) {
				  variant_match(proj.storage) {
					  variant_case(FieldProjection, field) {
						  const auto& class_layout
							  = std::get<tsl::ClassTypeLayout>(current_layout->getVariant());
						  const auto layout_idx
							  = class_layout.getLayoutIndexOfFieldSymbol(field.field_id).value();
						  current_layout = class_layout.getFieldLayoutOfLayoutIndex(layout_idx);
					  }
					  variant_case(IndexProjection, index) {
						  variant_match(current_layout->getVariant()) {
							  variant_case(tsl::StaticArrayTypeLayout, static_array_layout) {
								  current_layout = static_array_layout.getElementLayout();
							  }
							  variant_case(tsl::DynamicArrayTypeLayout, dynamic_array_layout) {
								  current_layout = dynamic_array_layout.getElementLayout();
							  }
							  variant_default {
								  CORE_PANIC(
									  "Cannot index into non-array type in LIR: Current layout: ",
									  current_layout->toStringIdentification()
								  );
							  }
						  }
					  }
					  variant_case_novalue(DerefProjection) {
						  const auto& pointer_layout
							  = std::get<tsl::PointerTypeLayout>(current_layout->getVariant());
						  current_layout = pointer_layout.getPointee();
					  }
				  }
			  }
			  return current_layout;
		  }()),
		  projection_chain(std::move(projection_chain)) {}

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
				if (*local.layout != *parameter_layouts.at(index)) return base::BAD;
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
			output << "    LAYOUT:" << local->layout->toStringDefinition(ctx, true, 1) << "\n";
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

		void printChain(const std::vector<LIRPlace::Projection>& chain, std::ostream& loc_output) {
			for (const auto& proj: chain) {
				variant_match(proj.storage) {
					variant_case(LIRPlace::FieldProjection, field) {
						loc_output << "." << name(field.field_id).strView();
					}
					variant_case_novalue(LIRPlace::DerefProjection) { loc_output << ".*"; }
				}
			}
		}

		void printOutput(const LIRPlace& output, std::ostream& loc_output) {
			variant_match(output.base) {
				variant_case(LIRLocalRef, local) { printLocal(local, loc_output); }
				variant_case(LIRGlobal, global) { printGlobal(global, loc_output); }
			}
			printChain(output.projection_chain, loc_output);
		}

		void printValue(const LIRValue& location) {
			variant_match(location.getVariant()) {
				variant_case(LIRConstant, constant) { output << constant.value.toString(); }
				variant_case(LIRPlace, place) {
					variant_match(place.base) {
						variant_case(LIRLocalRef, local) { printLocal(local, output); }
						variant_case(LIRGlobal, global) { printGlobal(global, output); }
					}

					printChain(place.projection_chain, output);
				}
				variant_case(BlockRef, block) { output << "Block(" << block_id[block] << ")"; }
				variant_case(FunctionLiteral, func) {
					output << "Func(" << func.mangled_name.strView() << ")";
				}
				variant_default { CORE_PANIC("Unhandled variant in printValue"); }
			}
		}

		void printInstructionExtraParams(const InstrParameters& instr_params) {
			output << "  ";
			variant_match(instr_params) {
				variant_case_novalue(NoInstrParameters) { /* nothing */ }
				variant_case(CastParameters, params) {
					output << "{ from:" << params.source_type.toString()
						   << ", to:" << params.target_type.toString() << " }";
				}
				variant_case(ListOperationParameters, params) {
					output << "{ element_layout:" << params.element_layout->toStringIdentification()
						   << " }";
				}
			}
			output << " ";
		}

		void printInstruction(const Instruction& instruction) {
			// save flags to restore
			auto output_flags = output.flags();

			output << std::left << std::setw(3);
			std::stringstream output_value;
			if (instruction.output.has_value()) {
				printOutput(instruction.output.value(), output_value);
				output_value << " :=";
			}
			output << output_value.str() << ' ';
			output << std::left << std::setw(15);
			output << base::enumToStr(instruction.operation) << "  ";
			if (!instruction.output.has_value()) output << std::left << std::setw(12);

			std::string_view sep = "";
			for (const auto& arg: instruction.arguments) {
				output << sep;
				sep = ", ";
				printValue(arg);
			}

			// restore flags
			output.flags(output_flags);

			printInstructionExtraParams(instruction.extra_params);
		}

		void debugPrint(const Function& function) {
			// set local and block ids:
			local_id = function.getLocalVariableIDs();
			block_id = function.getBlockIDs();

			output << "[LIR] Function \"" << function.mangled_name.strView() << "\":\n";

			for (const auto& local: function.local_list) {
				printLocalDesc(&local);
				output << '\n';
			}
			output << "{\n";

			for (auto block: function.block_order) {
				output << "  Block " << block_id[block] << ":\n";

				for (const auto& instruction: block->instructions) {
					output << "    ";
					printInstruction(instruction);
					output << '\n';
				}
				printInstruction(block->terminator);
				output << '\n';
			}

			output << "}\n";
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
