#include "lir_structure.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <tsl/queries.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
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
	 * When a function is provided, their printed IDs match the IDs assigned by that function.
	 * Otherwise IDs are assigned in encounter order and are meaningful only within a single print
	 * operation.
	 */
	struct LIRPrinter final {
		std::ostream&                       output;
		base::Optional<Ref<query::Context>> ctx;

		base::Map<LIRLocalRef, usize> local_id;
		base::Map<BlockRef, usize>    block_id;
		std::string_view              id_prefix = "?";

		explicit LIRPrinter(
			std::ostream&                       output,
			base::Optional<Ref<query::Context>> ctx      = {},
			base::Optional<CRef<Function>>      function = {}
		):
			  output(output),
			  ctx(ctx) {
			if (function.has_value()) useFunctionIDs(*function.value());
		}

		usize getLocalID(LIRLocalRef local) {
			if (local_id.contains(local)) return local_id[local];

			const usize id = local_id.size();
			local_id.put(local, id);
			return id;
		}

		usize getBlockID(BlockRef block) {
			if (block_id.contains(block)) return block_id[block];

			const usize id = block_id.size();
			block_id.put(block, id);
			return id;
		}

		void useFunctionIDs(const Function& function) {
			local_id  = function.getLocalVariableIDs();
			block_id  = function.getBlockIDs();
			id_prefix = "";
		}

		void printLocalDesc(LIRLocalRef local) {
			output << "  Local(" << getLocalID(local) << ")";
			if (local->helios_id.has_value())
				output << ", helios_name: " << name(local->helios_id.value()).strView();
			if (local->parameter_index.has_value())
				output << ", parameter_index: " << local->parameter_index.value();
			if (local->special_kind != LIRLocalSpecialKind::Normal)
				output << ", kind: " << base::enumToStr(local->special_kind);
			output << "\n";
			output << "    LAYOUT:";
			if (ctx.has_value())
				output << local->layout->toStringDefinition(*ctx.value(), true, 1);
			else
				output << "\t" << local->layout->toStringIdentification();
			output << '\n';
		}

		/**
		 * @note Custom output, so we can align when printing instruction
		 */
		void printLocal(LIRLocalRef local, std::ostream& loc_output) {
			loc_output << "Local(" << id_prefix << getLocalID(local) << ")";
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
					variant_case(LIRPlace::IndexProjection, index) {
						loc_output << "[";
						printValue(*index.index, loc_output);
						loc_output << "]";
					}
					variant_case_novalue(LIRPlace::DerefProjection) { loc_output << ".*"; }
					variant_default { CORE_UNREACHABLE(); }
				}
			}
		}

		void printPlace(const LIRPlace& place, std::ostream& loc_output) {
			variant_match(place.base) {
				variant_case(LIRLocalRef, local) { printLocal(local, loc_output); }
				variant_case(LIRGlobal, global) { printGlobal(global, loc_output); }
			}
			printChain(place.projection_chain, loc_output);
		}

		void printValue(const LIRValue& value, std::ostream& loc_output) {
			variant_match(value.getVariant()) {
				variant_case(LIRConstant, constant) { loc_output << constant.value.toString(); }
				variant_case(LIRPlace, place) { printPlace(place, loc_output); }
				variant_case(BlockRef, block) {
					loc_output << "Block(" << id_prefix << getBlockID(block) << ")";
				}
				variant_case(FunctionLiteral, func) {
					loc_output << "Func(" << func.mangled_name.strView() << ")";
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
				printPlace(instruction.output.value(), output_value);
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
				printValue(arg, output);
			}

			// restore flags
			output.flags(output_flags);

			printInstructionExtraParams(instruction.extra_params);
			printInstructionLifetimeFlags(instruction.scope_flags);
		}

		void printFunction(const Function& function) {
			useFunctionIDs(function);

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

	void Function::debugPrint(std::ostream& output, base::Optional<Ref<query::Context>> ctx) const {
		LIRPrinter{ output, ctx }.printFunction(*this);
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

	void LIRGlobal::debugPrint(std::ostream& os, base::Optional<Ref<query::Context>> ctx) const {
		os << "[LIR] Global ";
		os << (type == LIRGlobalType::Constant ? "constant" : "variable") << ": ";
		os << mangled_name.strView() << "\n";
		if (ctx.has_value())
			os << "Type: " << layout->toStringDefinition(*ctx.value()) << "\n";
		else
			os << "Type: " << layout->toStringIdentification() << '\n';
	}

	void LIRGlobalData::debugPrint(std::ostream& os, base::Optional<Ref<query::Context>> ctx) const {
		global.debugPrint(os, ctx);
		os << "  Data Initialization: ";

		variant_match(data_initialization) {
			variant_case(ctv::CompileTimeValue, ctv) { os << ctv.toString(); }
			variant_case(CTorDtorPair, ctor_dtor_pair) {
				os << "\n";
				if_opt_some(ctor_dtor_pair.global_ctor, ctor) {
					os << "  Global constructor:\n";
					ctor->debugPrint(os, ctx);
				}
				if_opt_some(ctor_dtor_pair.global_dtor, dtor) {
					os << "  Global destructor:\n";
					dtor->debugPrint(os, ctx);
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

	void LIRUnit::debugPrint(std::ostream& os, base::Optional<Ref<query::Context>> ctx) const {
		os << "LIRUnit: \n";
		os << "Globals:\n";
		for (const auto& global: lir_globals) {
			global.debugPrint(os, ctx);
			os << "\n";
		}
		os << "Functions:\n";
		for (const auto& func: lir_functions) {
			func->debugPrint(os, ctx);
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
			return true;
		default:
			return false;
		}
	}

	void LIRPlace::debugPrint(std::ostream& output, base::Optional<CRef<Function>> function) const {
		LIRPrinter{ output, {}, function }.printPlace(*this, output);
	}

	void LIRValue::debugPrint(std::ostream& output, base::Optional<CRef<Function>> function) const {
		LIRPrinter{ output, {}, function }.printValue(*this, output);
	}

	void Instruction::debugPrint(std::ostream& output, base::Optional<CRef<Function>> function)
		const {
		LIRPrinter{ output, {}, function }.printInstruction(*this);
	}
}
