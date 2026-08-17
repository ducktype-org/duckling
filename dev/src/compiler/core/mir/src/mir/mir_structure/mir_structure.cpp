#include "mir_structure.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>

#include <iomanip>
#include <sstream>
#include <variant>

namespace compiler::mir {
	Function::Function(
		base::StrID                                         name,
		tsh::SymbolType<>                                   return_type,
		std::vector<tsh::SymbolType<>>                      parameter_types,
		base::StableHashMap<BlockID, Block>                 blocks,
		std::vector<BlockID>                                block_order,
		base::StableVector<const MIRLocal>                  local_list,
		LifetimeScopeTree                                   lifetime_scope_tree,
		ScopeRef                                            no_lifetime_scope,
		std::variant<FunctionSymID, GlobalVariableCtorDtor> helios_id
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

	u64 Function::queryUnstablePerfectHash() const {
		// There should be no collisions possible here, since both FunctionSymID and
		// GlobalVariableCtorDtor just store SymID, which has a perfect hash.
		variant_match(this->helios_id) {
			variant_case(FunctionSymID, fun_sym) { return fun_sym.id.queryUnstablePerfectHash(); }
			variant_case(GlobalVariableCtorDtor, global_ctor_dtor) {
				// The highest bit distinguishes the ctor from the dtor of the same global,
				// SymID hashes are indices, so they never reach it.
				const u64 dtor_bit = global_ctor_dtor.type == GlobalVariableCtorDtor::Type::Dtor
				                       ? u64(1) << 63
				                       : 0;
				return global_ctor_dtor.global_var_id.queryUnstablePerfectHash() | dtor_bit;
			}
		}
		CORE_UNREACHABLE();
	}

	MIRLocalRef Function::addGeneratedLocal(
		tsh::SymbolType<> type, ScopeRef scope, LifetimeFlags flags
	) {
		u64 next_id = 0;
		for (const auto& local: local_list)
			if (local.id.asInt() >= next_id) next_id = local.id.asInt() + 1;

		MIRLocal local{ LocalID(next_id), type };
		local.scope          = scope;
		local.lifetime_flags = flags;

		local_list.pushBack(local);
		return local_list.last();
	}

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

	void Function::debugPrint(std::ostream& os) const {
		os << "[MIR] Function " << name.strView() << ": ";
		os << "(";
		std::string_view separator = "";
		for (auto& type: this->parameter_types) {
			os << separator << type.toString();
			separator = ", ";
		}
		os << ")";
		os << " -> " << this->return_type.toString() << "\n";

		for (auto& local: this->local_list) {
			os << "    ";
			local.debugPrint(os, true);
			os << "\n";
		}
		os << "No Lifetime Scope: " << no_lifetime_scope->id << "\n";
		os << "{\n";

		for (const auto block_id: block_order) {
			const auto& block = blocks[block_id];

			os << "  Block " << u64(block.id);
			if (block.id == block_order[0]) os << " [entry]";
			os << ":\n";
			for (const auto& instruction: block.instructions) {
				os << "    ";
				instruction.debugPrint(os);
				os << "\n";
			}
			os << "    ";
			block.terminator.debugPrint(os);
			os << "\n";
		}
		os << "}\n";
	}

	void debugPrintInstrParameters(std::ostream& os, const InstrParameters& instr_params) {
		os << " Params{";
		variant_match(instr_params) {
			variant_case_novalue(NoInstrParameters) { /* nothing */ }
			variant_case(CastParameters, params) {
				os << "from:" << params.source_type.toString()
				   << ", to:" << params.target_type.toString();
			}
		}
		os << "}";
	}

	void Instruction::debugPrint(std::ostream& os) const {
		// save flags to restore
		auto output_flags = os.flags();

		os << std::left << std::setw(13);
		if (output.has_value()) {
			output.value().debugPrint(os);
			os << " :=";
		}
		os << " ";

		std::string op_name{ base::enumToStr(operation) };
		if (operation == Operation::MetaTypeOperation)
			if (const auto* meta_params = std::get_if<MetaParameters>(&extra_params))
				op_name += ":" + std::string{ base::enumToStr(meta_params->kind) };
		os << std::left << std::setw(15);
		os << op_name << "  ";

		std::stringstream args;

		std::string_view separator = "";
		for (const auto& arg: arguments) {
			args << separator;
			arg.debugPrint(args);
			separator = ", ";
		}

		os << std::left << std::setw(15);
		os << args.str() << "  ";

		separator = "";
		os << "Flags[";
		for (const auto& flag: flags) {
			os << separator;
			flag.debugPrint(os);
			separator = ", ";
		}
		os << "], ";
		debugPrintInstrParameters(os, extra_params);
		os << ", scope:" << scope->id;

		// restore flags
		os.flags(output_flags);
	}

	void MIRLocal::debugPrint(std::ostream& os, bool detailed) const {
		os << std::setw(0);
		os << "Local(" << u64(id) << ")";
		if (detailed) {
			os << ": Helios Name: " << getName().strView();
			os << ", Type: " << type.toString();
			os << ", Lifetime Scope: " << scope.value()->id;
			os << ", Lifetime Flags: " << lifetime_flags.toString(true);
			if (parameter_index.has_value()) os << ", Parameter Index: " << parameter_index.value();
		}
	}

	void MIRGlobal::debugPrint(std::ostream& os, bool detailed) const {
		if (detailed) {
			os << "[MIR] Global: ";
			os << ": Helios Name: " << name(helios_id).strView();
			os << ", Type: " << type.toString();
			os << "\n";
		}
		if (not detailed) os << "Global(" << name(helios_id).strView() << ")";
	}

	base::StrID MIRLocal::getName() const {
		if (helios_id.has_value()) return name(helios_id.value());
		return base::StrID(base::strConcat(id.asInt(), ".tmp"));
	}

	void MIRLocal::setLifetimeScope(ScopeRef scope) {
		CORE_ASSERT(this->scope.empty(), "lifetime_scope is already set");
		this->scope.emplace(scope);
	}

	MIRPlace MIRPlace::withDeref() const {
		MIRPlace result = *this;

		result.projection_chain.push_back(Projection::deref());
		// New type after deref is the one which was referenced by the ref/box, without the
		// reference specifier.
		result.type = result.type.getPointeeSymbolType();
		return result;
	}

	MIRPlace MIRPlace::withField(query::Context& ctx, const helios::SymID field) const {
		MIRPlace result = *this;
		CORE_ASSERT(
			result.type.getRefKind() == tsh::ReferenceKind::Direct,
			"Field access on ref/box type. A proper DerefExpr should be inserted in HOUT"
		);
		result.projection_chain.push_back(Projection::field(field));
		result.type = ctx.query<helios::QueryTypeOfSymbol>(field)->valueOrThrow();
		return result;
	}

	MIRPlace MIRPlace::withIndex(const MIRValue& index) const {
		MIRPlace result = *this;

		auto base_type = type.getType();

		auto element_type = [&]() -> tsh::SymbolType<> {
			switch (base_type.getKind()) {
			case tsh::Kind::DynamicArray:
				return base_type.as<tsh::DynamicArrayAbstractType>().getElementType();
			case tsh::Kind::StaticArray:
				return base_type.as<tsh::StaticArrayAbstractType>().getElementType();
			case tsh::Kind::ManyPointer:
				return base_type.as<tsh::ManyPointerAbstractType>().getPointee();
			case tsh::Kind::CPointer:
				return base_type.as<tsh::CPointerAbstractType>().getPointee();
			default:
				CORE_PANIC("Cannot index into type: ", type.toString());
			}
		}();

		result.projection_chain.push_back(Projection::index(index));
		result.type = element_type;
		return result;
	}

	void MIRPlace::debugPrint(std::ostream& os, bool detailed) const {
		variant_match(base) {
			variant_case(MIRLocalRef, local) { local->debugPrint(os, detailed); }
			variant_case(MIRGlobal, global) { global.debugPrint(os, detailed); }
		}

		for (const auto& proj: projection_chain) {
			variant_match(proj.storage) {
				variant_case(FieldProjection, field) {
					os << "." << name(field.field_id).strView();
				}
				variant_case(IndexProjection, index) {
					os << "[";
					index.index->debugPrint(os);
					os << "]";
				}
				variant_case_novalue(DerefProjection) { os << ".*"; }
			}
		}

		if (detailed) {
			os << ": Type: ";
			os << type.toString();
		}
	}

	void MIRValue::debugPrint(std::ostream& os) const {
		variant_match(value) {
			variant_case(MIRConstant, value) { os << value.value.toString(); }
			variant_case(MIRPlace, place) { place.debugPrint(os); }
			variant_case(BlockID, block) { os << "Block(" << u64(block) << ")"; }
			variant_case(MIRFunctionLiteral, func) {
				os << "Function(" << name(func.helios_id).strView() << ")";
			}
			variant_default { CORE_PANIC("Unexpected MIRLocal alternative in mir debugPrint"); }
		}
	}

	void OperationFlag::debugPrint(std::ostream& os) const {
		switch (flag) {
		case Flag::Construct:
			os << "Construct";
			break;
		case Flag::Reinit:
			os << "Reinit";
			break;
		case Flag::Destruct:
			os << "Destruct";
			break;
		case Flag::Move:
			os << "Move";
			break;
		case Flag::ScopeStart:
			os << "ScopeStart";
			break;
		case Flag::ScopeEnd:
			os << "ScopeEnd";
			break;
		}
		os << " ";
		local->debugPrint(os);
	}

	void MIRGlobalData::debugPrint(query::Context&, std::ostream& os) const {
		global.debugPrint(os, true);
		os << "  Initial Value (CTV or Function): ";
		variant_match(initial_value) {
			variant_case(ctv::CompileTimeValue, ctv) { os << ctv.toString(); }
			variant_case(MIRCtorDtorPair, pair) {
				os << "constructor: " << pair.constructor->name.strView() << "\n";
				pair.constructor->debugPrint(os);
				if_opt_some(pair.destructor, dtor) {
					os << "destructor: " << dtor->name.strView() << "\n";
					dtor->debugPrint(os);
				}
			}
		}
	}

	void MIRUnit::debugPrint(query::Context& ctx, std::ostream& os) const {
		os << "MIRUnit:\n";
		os << "Globals:\n";
		for (const auto& global: mir_globals) {
			global->debugPrint(ctx, os);
			os << "\n";
		}
		os << "Functions:\n";
		for (const auto& func: mir_functions) {
			func->debugPrint(os);
			os << "\n";
		}
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

	Instruction& Block::firstInstruction() {
		if (instructions.empty()) return terminator;
		return instructions.front();
	}
}
