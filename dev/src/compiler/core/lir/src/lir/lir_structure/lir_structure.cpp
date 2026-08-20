#include "lir_structure.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <tsl/queries.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>

#include <iomanip>
#include <unordered_set>

namespace compiler::lir {
	LIRLocal LIRLocal::fromMIR(
		query::Context&           ctx,
		const mir::MIRLocalRef    mir_local,
		const base::Optional<u64> new_parameter_index
	) {
		auto type_layout
			= CRef<tsl::TypeLayout>(&ctx.query<tsl::QuerySymbolTypeLayout>(mir_local->type)
		                                 ->valueOrPanicMsg("layout query failed at LIR stage"));
		LIRLocalMetadata metadata;
		if_opt_some(mir_local->helios_id, helios_id) {
			metadata.source_code_name = helios::name(helios_id);
			if_opt_some(helios::maybeSymbolPst(helios_id), pst_elem) {
				metadata.position = pst_elem.unlock(ctx)->getStablePosition();
			}
		}
		LIRLocalSpecialKind special_kind = specialKindFromMIR(*mir_local);
		return LIRLocal{
			mir_local->helios_id, type_layout, new_parameter_index, metadata, special_kind
		};
	}

	LIRLocal LIRLocal::refLocal(query::Context& ctx, const tsh::SymbolType<> pointee_type) {
		const auto ref_type   = pointee_type.withReferenceKind(tsh::ReferenceKind::Ref);
		auto&      ref_layout = ctx.query<tsl::QuerySymbolTypeLayout>(ref_type)->valueOrPanicMsg(
            "layout query failed at LIR stage"
        );

		return LIRLocal{ CRef<tsl::TypeLayout>(&ref_layout) };
	}

	LIRGlobal LIRGlobal::fromMIR(query::Context& ctx, mir::MIRGlobal mir_global) {
		auto type_layout
			= CRef<tsl::TypeLayout>(&ctx.query<tsl::QuerySymbolTypeLayout>(mir_global.type)
		                                 ->valueOrPanicMsg("layout query failed at LIR stage"));

		auto mangled_name  = helios::mangler::getSimpleMangledName(ctx, mir_global.helios_id);
		LIRGlobalType type = mir_global.kind == mir::MIRGlobal::Kind::Constant
		                       ? LIRGlobalType::Constant
		                       : LIRGlobalType::Variable;

		const bool link_once = helios::emissionPolicy(ctx, mir_global.helios_id)
		                    == helios::EmissionPolicy::Replicated;

		return LIRGlobal{
			type_layout,
			mangled_name,
			type,
			link_once,
		};
	}

	LIRPlace::LIRPlace(BaseVariant base, std::vector<Projection> projection_chain):
		  base(base),
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
							  variant_case(tsl::PointerTypeLayout, many_pointer_layout) {
								  current_layout = many_pointer_layout.getPointee();
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
	struct LIRPrinter final {
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
			if (local->special_kind != LIRLocalSpecialKind::Normal)
				output << ", kind: " << base::enumToStr(local->special_kind);
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
				variant_case(VariantParameters, params) {
					output << "{ alt:" << params.alternative_index << " ("
						   << params.alternative_type.toString() << ") }";
				}
			}
			output << " ";
		}

		void printInstructionLifetimeFlags(const std::vector<ScopeFlag>& flags) {
			if (flags.empty()) return;

			output << " [";
			for (const auto& flag: flags) {
				std::string flag_str
					= (flag.flag == ScopeFlag::Flag::ScopeStart) ? "ScopeStart" : "ScopeEnd";
				output << flag_str << "(";
				printLocal(flag.local, output);
				output << "), ";
			}
			output << "] ";
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
			std::string op_name{ base::enumToStr(instruction.operation) };
			if (instruction.operation == Operation::MetaTypeOperation)
				if (const auto* meta_params = std::get_if<MetaParameters>(&instruction.extra_params))
					op_name += ":" + std::string{ base::enumToStr(meta_params->kind) };
			output << std::left << std::setw(15);
			output << op_name << "  ";
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
			printInstructionLifetimeFlags(instruction.scope_flags);
		}

		void debugPrint(const Function& function) {
			// set local and block ids:
			local_id = function.getLocalVariableIDs();
			block_id = function.getBlockIDs();

			output << "[LIR] Function \"" << function.mangled_name.strView() << "\""
				   << (function.link_once ? " (link once)" : "")
				   << (function.ignore_on_dvm ? " (ignore on dvm)" : "")
				   << (function.ignore_on_llvm ? " (ignore on llvm)" : "") << "\n";

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
			.link_once    = function.link_once,
			.parameter_layouts
			= std::make_shared<std::vector<CRef<tsl::TypeLayout>>>(function.parameter_layouts),
			.return_type_layout = function.return_type_layout,
			.builtin_kind_opt   = {},
		};
	}

	void LIRGlobal::debugPrint(query::Context& ctx, std::ostream& os) const {
		os << "[LIR] Global ";
		os << (type == LIRGlobalType::Constant ? "constant" : "variable") << ": ";
		os << mangled_name.strView() << "\n";
		os << "Type: " << layout->toStringDefinition(ctx) << "\n";
	}

	void LIRGlobalData::debugPrint(query::Context& ctx, std::ostream& os) const {
		global.debugPrint(ctx, os);
		os << "  Data Initialization: ";
		variant_match(data_initialization) {
			variant_case(ctv::CompileTimeValue, ctv) { os << ctv.toString(); }
			variant_case(CTorDtorPair, ctor_dtor_pair) {
				if_opt_some(ctor_dtor_pair.global_ctor, ctor) {
					os << "  Global constructor:\n";
					ctor->debugPrint(ctx, os);
				}
				if_opt_some(ctor_dtor_pair.global_dtor, dtor) {
					os << "  Global destructor:\n";
					dtor->debugPrint(ctx, os);
				}
			}
		}
	}

	LIRGlobalData::CTorDtorPair LIRGlobalData::getCtorDtorPair() const {
		CORE_ASSERT(
			std::holds_alternative<LIRGlobalData::CTorDtorPair>(data_initialization),
			"Global does not have constructor/destructor initialization: ",
			global.mangled_name.strView()
		);
		return std::get<LIRGlobalData::CTorDtorPair>(data_initialization);
	}

	ctv::CompileTimeValue LIRGlobalData::getConstValue() const {
		CORE_ASSERT(
			std::holds_alternative<ctv::CompileTimeValue>(data_initialization),
			"Global does not have constant initialization: ",
			global.mangled_name.strView()
		);
		return std::get<ctv::CompileTimeValue>(data_initialization);
	}

	void LIRUnit::debugPrint(query::Context& ctx, std::ostream& os) const {
		os << "LIRUnit: \n";
		os << "Globals:\n";
		for (const auto& global: lir_globals) {
			global.debugPrint(ctx, os);
			os << "\n";
		}
		os << "Functions:\n";
		for (const auto& func: lir_functions) {
			func->debugPrint(ctx, os);
			os << "\n";
		}
	}

	void LIRUnit::deduplicateSymbols() {
		base::deduplicateBy(lir_globals, [](const LIRGlobalData& global_data) {
			return global_data.global.mangled_name;
		});

		std::unordered_set<base::StrID> seen_on_dvm;
		std::unordered_set<base::StrID> seen_on_llvm;
		std::vector<CRef<Function>>     result_functions;
		for (auto lir_func: lir_functions) {
			const bool needed_on_dvm
				= not lir_func->ignore_on_dvm and seen_on_dvm.insert(lir_func->mangled_name).second;
			const bool needed_on_llvm = not lir_func->ignore_on_llvm
			                        and seen_on_llvm.insert(lir_func->mangled_name).second;

			if (needed_on_dvm or needed_on_llvm) result_functions.push_back(lir_func);
		}
		lir_functions = std::move(result_functions);
	}

	base::Optional<BuiltinFunctionKind> getBuiltinKindFromHOUT(helios::BuiltinKind kind) {
		switch (kind) {
		case helios::BuiltinKind::DvmAllocArr:
			return BuiltinFunctionKind::DvmAllocArr;
		case helios::BuiltinKind::DvmReallocArr:
			return BuiltinFunctionKind::DvmReallocArr;
		case helios::BuiltinKind::DvmFreeArr:
			return BuiltinFunctionKind::DvmFreeArr;
		case helios::BuiltinKind::DvmAlloc:
			return BuiltinFunctionKind::DvmAlloc;
		case helios::BuiltinKind::DvmFree:
			return BuiltinFunctionKind::DvmFree;
		case helios::BuiltinKind::BoxAlloc:
			return BuiltinFunctionKind::BoxAlloc;
		case helios::BuiltinKind::BoxFree:
			return BuiltinFunctionKind::BoxFree;
		default:
			return {};
		}
	}

	LIRLocalSpecialKind specialKindFromMIR(const mir::MIRLocal& mir_local) {
		if (mir_local.lifetime_flags.contains(mir::LifetimeFlag::ReturnTmpValue))
			return LIRLocalSpecialKind::ReturnValue;
		else if (mir_local.lifetime_flags.contains(mir::LifetimeFlag::ConditionTmpValue))
			return LIRLocalSpecialKind::ConditionTmp;
		else
			return LIRLocalSpecialKind::Normal;
	}

	[[nodiscard]] bool Instruction::isTerminating() const {
		switch (operation) {
		case Operation::ReturnVoid:
		case Operation::ReturnValue:
		case Operation::Jump:
		case Operation::Branch:
		case Operation::BranchIfNull:
		case Operation::Unreachable:
			return true;
		default:
			return false;
		}
	}
}
