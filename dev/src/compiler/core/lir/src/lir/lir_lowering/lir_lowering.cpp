/**
 * @file lir_lowering.cpp
 * @brief File implementing process of creating LIR function from MIR function
 *
 * @note when adding new cases to logic in this file you should most likely edit:
 * - mir2lirOperation -- for new operations
 * - Mir2Lir::getLocation -- for new location
 * - Mir2Lir::lowerFlags -- for flag handling
 * - Mir2Lir::lowerInstruction -- for new instructions
 * - Mir2Lir::lowerTerminator -- for new terminators
 * - LowerToLirFunction::provide -- for some new steps
 */

#include "lir_lowering.hpp"

#include "../lir_structure/lir_structure.hpp"

#include <helios/mangler/mangler.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/hout/hout.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries.hpp>
#include <typesystem/lower/queries.hpp>

#include <base/variant.hpp>

#include <query_framework/query_impl.hpp>

#include <utility>

// @opt: make switch-cases in this file "sorted"

namespace compiler::lir {

	/**
	 * @brief Mutable reference block in LIR.
	 */
	using MutBlockRef = Ref<Block>;

	u64 KeyOf_LowerToLirFunction::queryUnstablePerfectHash() const {
		return function->queryUnstablePerfectHash();
	}

	FunctionLiteral getFunctionLiteralfromHELIOSID(query::Context& ctx, helios::SymID helios_id);

	/**
	 * @brief Creates LIR local data from MIR local data.
	 * @todo change argument to MIR local reference.
	 * @important remember that LirLocal should only be stored in a LIR function.
	 *
	 * @param ctx
	 * @param mir_local
	 * @return LirLocal
	 */
	LirLocal LirLocal::fromMIR(query::Context& ctx, mir::LocalRef mir_local) {
		auto type_layout = ctx.query<tsl::QuerySymbolTypeLayout>(mir_local->type);

		return LirLocal{ mir_local->helios_id, type_layout, mir_local->parameter_index };
	}

	LirLocal LirLocal::boolLocal(query::Context& ctx) {
		auto bool_type   = ctx.query<tsh::QueryBoolType>({});
		auto bool_layout = ctx.query<tsl::QueryAbstractTypeLayout>(bool_type);

		return LirLocal{ bool_layout };
	}

	LirGlobal LirGlobal::fromMIR(query::Context& ctx, mir::MirGlobal mir_global) {
		auto type_layout = ctx.query<tsl::QuerySymbolTypeLayout>(mir_global.type);

		auto mangled_name = helios::mangler::getSimpleMangledName(ctx, mir_global.helios_id);

		return LirGlobal{ mir_global.helios_id, type_layout, mangled_name };
	}

	LirGlobal LirGlobal::fromHOUT(query::Context& ctx, const helios::HOUTGlobalData& hout_global) {
		auto type_layout = ctx.query<tsl::QuerySymbolTypeLayout>(hout_global.type);

		auto mangled_name = helios::mangler::getSimpleMangledName(ctx, hout_global.helios_symbol);

		variant_match(hout_global.value) {
			variant_case(helios::HOUTGlobalConst, name) {
				return LirGlobal{
					hout_global.helios_symbol, type_layout, mangled_name,
					LirGlobalType::Constant,   name.value,
				};
			}
			variant_case(helios::HOUTGlobalVariable, name) {
				return LirGlobal{
					hout_global.helios_symbol, type_layout, mangled_name, LirGlobalType::Variable
				};
			}
			variant_default {
				CORE_PANIC(
					"Unhandled HOUTGlobalData type in LirGlobal::fromHOUT: ",
					hout_global.original_name.strView()
				);
			}
		}

		CORE_UNREACHABLE();
	}

	/**
	 * @brief Maps MIR operation to LIR operation for those
	 * that have direct counterpart.
	 *
	 * @param mir_operation The MIR operation to convert.
	 * @param signed_version Whether to use a signed version of the operation, if relevant.
	 * @return Operation
	 */
	Operation mir2lirOperation(const mir::Operation mir_operation, const bool signed_version) {
		switch (mir_operation) {
		case mir::Operation::Assign:
			return Operation::Assign;
		case mir::Operation::ReturnValue:
			return Operation::ReturnValue;
		case mir::Operation::ReturnVoid:
			return Operation::ReturnVoid;
		case mir::Operation::Jump:
			return Operation::Jump;
		case mir::Operation::Branch:
			return Operation::Branch;
		case mir::Operation::IntegerAdd:
			return Operation::IntegerAdd;
		case mir::Operation::IntegerSub:
			return Operation::IntegerSub;
		case mir::Operation::IntegerMul:
			return Operation::IntegerMul;
		case mir::Operation::IntegerDiv:
			return signed_version ? Operation::IntegerSDiv : Operation::IntegerUDiv;
		case mir::Operation::IntegerMod:
			return signed_version ? Operation::IntegerSMod : Operation::IntegerUMod;

		case mir::Operation::IntegerLt:
			return signed_version ? Operation::IntegerSLt : Operation::IntegerULt;
		case mir::Operation::IntegerGt:
			return signed_version ? Operation::IntegerSGt : Operation::IntegerUGt;
		case mir::Operation::IntegerLteq:
			return signed_version ? Operation::IntegerSLteq : Operation::IntegerULteq;
		case mir::Operation::IntegerGteq:
			return signed_version ? Operation::IntegerSGteq : Operation::IntegerUGteq;
		case mir::Operation::IntegerEq:
			return Operation::IntegerEq;
		case mir::Operation::IntegerNeq:
			return Operation::IntegerNeq;

		case mir::Operation::IntegerNeg:
			return Operation::IntegerNeg;
		case mir::Operation::BooleanAnd:
			return Operation::BooleanAnd;
		case mir::Operation::BooleanOr:
			return Operation::BooleanOr;
		case mir::Operation::BooleanNot:
			return Operation::BooleanNot;
		// @TODO: add more cases
		default:
			CORE_PANIC("Operation without direct counterpart");
		}
	}

	struct IMPLEMENT_QUERY(LowerToLirFunction, Function) {
		/**
		 * @brief Helper struct for easy state encapsulation used.
		 * by LowerToLirFunction query.
		 * @note This is not a typical struct, and
		 * should be seen as a set of functions operating on some common state.
		 * Order of those functions matter, as they build components of LIR function
		 * step by step.
		 */
		struct Mir2Lir {
			Context& ctx;
			QKey     key;

			Mir2Lir(Context& ctx, QKey key): ctx(ctx), key(key) {}

			base::StableVector<LirLocal> locals;

			// locals mapping:
			base::Map<mir::LocalRef, LocalRef> mir_to_lir_local;
			base::Map<mir::LocalRef, LocalRef> mir_to_lifetime_flag;

			base::StableVector<Block> blocks;

			// blocks mapping:
			base::Map<mir::BlockID, MutBlockRef> mir_to_lir_block;

			std::vector<BlockRef> block_order;

			// helper functions:

			/**
			 * @brief Returns LIR local associated with given MIR local.
			 *
			 * @param mir_local
			 * @return LocalRef
			 */
			LocalRef getLocal(mir::LocalRef mir_local) { return mir_to_lir_local.at(mir_local); }

			LirGlobal getGlobal(mir::MirGlobal mir_global) {
				return LirGlobal::fromMIR(ctx, mir_global);
			}

			/**
			 * @brief Get the output LIR location for the given MIR location. Returns an empty
			 * optional if the provided MIR location is empty, or if it does not carry information.
			 * @param output The MIR location to convert, possibly empty.
			 * @return The corresponding LIR location, possibly empty.
			 */
			base::Optional<std::variant<LocalRef, LirGlobal>> getOutput(
				const base::Optional<std::variant<mir::LocalRef, mir::MirGlobal>>& output
			) {
				if (!output.has_value()) return {};

				variant_match(output.value()) {
					variant_case(mir::LocalRef, local) {
						if (!local->carriesInformation()) return {};
						return getLocal(local);
					}
					variant_case(mir::MirGlobal, global) {
						if (!global.carriesInformation()) return {};
						return getGlobal(global);
					}
				}

				CORE_UNREACHABLE();
			}

			/**
			 * @brief Maps a single MIR location to an optional LIR location.
			 * @note Discards information-less locations, e.g. variables of unit type.
			 * @param loc The MIR location.
			 * @return The optional LIR location, possibly discarded.
			 */
			base::Optional<LIRValue> getLocation(const mir::MIRValue& loc) {
				// Discard information-less location.
				if (!loc.carriesInformation()) return {};

				variant_match(loc.getVariant()) {
					variant_case_novalue(mir::MirUnitConst) {
						CORE_PANIC("Cannot get location of MIR unit.");
					}
					variant_case(mir::MirIntegerConst, integer) {
						return LIRValue{ integer.value };
					}
					variant_case(mir::MirBoolConst, boolean) { return LIRValue{ boolean.value }; }
					variant_case(mir::LocalRef, local) { return LIRValue{ getLocal(local) }; }
					variant_case(mir::MirGlobal, global) { return LIRValue{ getGlobal(global) }; }
					variant_case(mir::BlockID, block) {
						return LIRValue{ BlockRef(mir_to_lir_block.at(block)) };
					}
					variant_case(mir::MirFunctionLiteral, func) {
						return getFunctionLiteralfromHELIOSID(ctx, func.helios_id);
					}
				}
				CORE_PANIC("Unhandled variant in getLocation");
			}

			// main functions:

			/**
			 * @brief Creates local vars data,
			 * puts them in a stable vector, and
			 * maps MIR locals to LIR local refs.
			 */
			void makeLocals() {
				for (const auto& mir_local: key.function->local_list) {
					// Discard data-less variables.
					if (!mir_local.carriesInformation()) continue;

					auto lir_local = LirLocal::fromMIR(ctx, &mir_local);
					locals.pushBack(std::move(lir_local));
					auto local_index = locals.lastIndex();
					mir_to_lir_local.put(&mir_local, locals[local_index]);

					// Only create lifetime flag if needed
					if (!mir_local.type.hasNoOpDestructor()) {
						auto lifetime_flag = LirLocal::boolLocal(ctx);
						locals.pushBack(std::move(lifetime_flag));
						auto flag_index = locals.lastIndex();
						mir_to_lifetime_flag.put(&mir_local, locals[flag_index]);
					}
				}
			}

			void makeInitialBlocks() {
				// make initial block mapping, and
				// unfilled blocks that will map to
				// beginning of each mir block
				for (const auto& mir_block_id: key.function->block_order) {
					// note that this block will only be filled with instructions
					// and terminator later:
					blocks.pushBack({});
					mir_to_lir_block.put(mir_block_id, blocks.last());
				}
			}

			void lowerBlocks() {
				for (const auto& mir_block_id: key.function->block_order) {
					const auto lir_block = mir_to_lir_block[mir_block_id];
					block_order.emplace_back(lir_block);

					auto        curr_block = lir_block;
					const auto& mir_block  = key.function->blocks[mir_block_id];
					for (const auto& mir_instruction: mir_block.instructions)
						curr_block = lowerInstruction(curr_block, mir_instruction);

					lowerTerminator(curr_block, mir_block.terminator);
				}
			}

			// instruction lowering functions:

			/**
			 * @brief Maps list of MIR locations to LIR locations.
			 * @note Discards information-less locations, e.g. variables of unit type.
			 *
			 * @param locs The MIR location.
			 * @return The LIR locations, possibly with some discarded.
			 */
			std::vector<LIRValue> getLocations(const std::vector<mir::MIRValue>& locs) {
				std::vector<LIRValue> result;
				result.reserve(locs.size());
				for (const auto& loc: locs)
					// Discard information-less locations.
					if (auto lir_loc = getLocation(loc); lir_loc.has_value())
						result.push_back(lir_loc.value());
				return result;
			}

			/**
			 * @brief Lowers flags of the given operation into
			 * LIR operations. Should always be called before lowering any operation
			 * @TODO: does calling before always make sense?
			 * @TODO: implement logic here
			 *
			 * @note: it is currently assumed this will not produce new blocks
			 * @param curr_block
			 * @param mir_instruction
			 */
			void lowerFlags(
				[[maybe_unused]] /*<temporary for linter*/ MutBlockRef curr_block,
				const mir::Instruction&                                mir_instruction
			) {
				for (const auto& [flag, local]: mir_instruction.flags) {
					// Discard flags for information-less locals.
					if (!local->carriesInformation()) continue;

					[[maybe_unused]]
					//< temporary for linter
					auto lir_local
						= getLocal(local);

					switch (flag) {
						using enum mir::OperationFlag::Flag;
					case Construct:
						// @TODO -- set lifetime flag
						return;
					case Destruct:
						// @TODO -- ??? (also: after or before...?)
						return;
					case Move:
						// @TODO -- unset lifetime flag
						return;
					default:
						throw base::NotYetImplemented("flag in lowerFlags");
					}
				}
			}

			static bool isArgSigned(const mir::MIRValue location) {
				variant_match(location.getVariant()) {
					variant_case_novalue(mir::MirIntegerConst) { return true; }
					variant_case(mir::LocalRef, local) {
						const auto arg_type = local->type.getType();
						return arg_type.getKind() == tsh::Kind::Integral
						   and tsh::IntegralAbstractType(arg_type).getSignedness()
						           == tsh::IntegralAbstractType::Signedness::Signed;
					}
					variant_default { return false; }
				}
				CORE_UNREACHABLE();
			}

			/**
			 * @brief Lowers instruction from MIR to LIR.
			 * * Fills @p curr_block.
			 * Legal to use only in lowerBlocks
			 * @param curr_block
			 * @return next curr_block
			 */
			MutBlockRef lowerInstruction(
				MutBlockRef curr_block, const mir::Instruction& mir_instruction
			) {
				// curr_block already in order

				CORE_ASSERT(
					not mir::isTerminating(mir_instruction.operation),
					"Terminator in lowerInstruction"
				);
				lowerFlags(curr_block, mir_instruction);

				switch (mir_instruction.operation) {
				case mir::Operation::Nop: {
					return curr_block;
				}
				case mir::Operation::Assign: {
					CORE_ASSERT(
						mir_instruction.arguments.size() == 1, "Assign should have one argument"
					);
					auto arg     = mir_instruction.arguments[0];
					auto lir_arg = getLocation(arg);
					// The assignment is discarded if it operates on no information.
					if (lir_arg.has_value()) {
						auto output = getOutput(mir_instruction.output);
						curr_block->instructions.emplace_back(
							Operation::Assign, output, std::vector{ lir_arg.value() }
						);
					}
					return curr_block;
				}
				case mir::Operation::IntegerAdd:
				case mir::Operation::IntegerNeg:
				case mir::Operation::IntegerSub:
				case mir::Operation::IntegerMul:
				case mir::Operation::IntegerDiv:
				case mir::Operation::IntegerMod:
				case mir::Operation::IntegerLt:
				case mir::Operation::IntegerGt:
				case mir::Operation::IntegerLteq:
				case mir::Operation::IntegerGteq:
				case mir::Operation::IntegerEq:
				case mir::Operation::IntegerNeq:
				case mir::Operation::BooleanAnd:
				case mir::Operation::BooleanOr:
				case mir::Operation::BooleanNot: {
					// this is a generic case, that will be used for most instructions
					// it currently assumes the output is present, but it can be changed
					auto output = getOutput(mir_instruction.output);

					auto args = getLocations(mir_instruction.arguments);

					// It is assumed that all arguments of a built-in function are of the same exact
					// type, and thus also have the same sign (if that matters). Any conversions
					// should have been handled by HELIoS.
					// @TODO: Refine this check.
					const auto use_signed_version = isArgSigned(mir_instruction.arguments.at(0));
					curr_block->instructions.emplace_back(
						mir2lirOperation(mir_instruction.operation, use_signed_version),
						output,
						std::move(args)
					);
					return curr_block;
				}
				case mir::Operation::DestructIf:
					// @TODO implement it, once we know how to call destructors
					std::cerr << "DestructIf not implemented in LIR, skipping" << "\n";
					return curr_block;
				case mir::Operation::Call: {
					auto output = getOutput(mir_instruction.output);
					auto args   = getLocations(mir_instruction.arguments);
					curr_block->instructions.emplace_back(Operation::Call, output, std::move(args));
					return curr_block;
				}
				default:
					throw base::NotYetImplemented(base::strConcat(
						"instruction ",
						base::enumToStr(mir_instruction.operation),
						" in LowerToLirFunction"
					));
				}
			}

			/**
			 * @brief Lowers terminator from MIR to LIR.
			 * Fills @p curr_block.
			 * Legal to use only in lowerBlocks
			 * @param curr_block
			 * @return next curr_block
			 */
			void lowerTerminator(MutBlockRef curr_block, const mir::Instruction& mir_terminator) {
				// @TODO
				// curr_block already in order
				CORE_ASSERT(
					mir::isTerminating(mir_terminator.operation), "non-Terminator in lowerTerminator"
				);
				lowerFlags(curr_block, mir_terminator);

				CORE_ASSERT(mir_terminator.output.empty(), "terminator should not return");
				switch (mir_terminator.operation) {
				case mir::Operation::ReturnValue: {
					// The returned value may have been discarded due to being information-less.
					if (auto args = getLocations(mir_terminator.arguments); args.size() == 0)
						curr_block->terminator = Instruction{ Operation::ReturnVoid, {}, {} };
					else
						curr_block->terminator
							= Instruction{ Operation::ReturnValue, {}, std::move(args) };
					break;
				}
				case mir::Operation::ReturnVoid:
				case mir::Operation::Jump:
				case mir::Operation::Branch: {
					auto args              = getLocations(mir_terminator.arguments);
					curr_block->terminator = Instruction{
						mir2lirOperation(mir_terminator.operation, false), {}, std::move(args)
					};
					break;
				}
				case mir::Operation::FunctionEnd: {
					CORE_PANIC("FunctionEnd is illegal outside of MirLowering phase");
					break;
				}
				// @TODO: add more cases
				default:
					throw base::NotYetImplemented("terminator in LowerToLirFunction");
				}
			}

			/**
			 * @brief Return function composed of generated data.
			 * Should be used once, at the end of LIR function creation.
			 * @return Function
			 */
			Function get() && {
				auto return_type = ctx.query<tsl::QuerySymbolTypeLayout>(key.function->return_type);
				std::vector<tsl::TypeLayout> parameter_types;
				parameter_types.reserve(key.function->parameter_types.size());
				for (const auto& param: key.function->parameter_types)
					// Discard information-less parameters from LIR function parameter lists.
					if (param.getType().carriesInformation())
						parameter_types.push_back(ctx.query<tsl::QuerySymbolTypeLayout>(param));


				auto abi = [&]() -> helios::SymbolABI {
					variant_match(key.function->helios_id) {
						variant_case(mir::FunctionSymID, name) {
							return ctx.query<helios::QuerySymbolABI>(name.id)->expect(
								"Handling errors in MIR is not supported yet"
							);
						}
						variant_case(mir::GlobalVariableCTOR, name) { return helios::DefaultAbi{}; }
					}
					CORE_UNREACHABLE();
				}();

				auto mangled_name = [&]() {
					variant_match(key.function->helios_id) {
						variant_case(mir::FunctionSymID, name) {
							variant_match(abi) {
								variant_case(helios::DefaultAbi, _) {
									return helios::mangler::getSimpleMangledName(ctx, name.id);
								}
								variant_case(helios::CAbi, _) { return helios::name(name.id); }
							}
						}
						variant_case(mir::GlobalVariableCTOR, name) {
							return helios::mangler::getSpecialMangledName<
								helios::mangler::ManglingSymbolKind::GlobalVariableConstructor>(
								ctx, name.global_var_id
							);
						}
					}
					CORE_UNREACHABLE();
				}();

				return Function{
					.mangled_name       = mangled_name,
					.abi                = abi,
					.return_type_layout = return_type,
					.parameter_layouts  = std::move(parameter_types),
					.blocks             = std::move(blocks),
					.local_list         = std::move(locals),
					.block_order        = std::move(block_order),
				};
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			// LirFunction is build-inplace here, and for this reason
			// achieves legal state only at the end.
			// for this reason additional assertions should be put inplace, to
			// ensure that the state is legal.

			// what we do here:
			// * map mir locals to lir locals
			// * map mir block numbers to lir blocks
			// * go thru all blocks
			// * generate lir-blocks from each mir-block, by:
			//   * going thru all instructions
			//   * generating lir-instructions and lir-blocks from each mir-instruction

			Mir2Lir mir2lir{ ctx, key };

			// call order matters:
			mir2lir.makeLocals();
			mir2lir.makeInitialBlocks();
			mir2lir.lowerBlocks();

			auto fun = std::move(mir2lir).get();

			// @opt: remove it in optimized, release builds
			CORE_ASSERT(fun.validateBlockOrder().isOk(), "Invalid block order");
			CORE_ASSERT(fun.validateParameters().isOk(), "Invalid parameters");

			return fun;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToLirFunction);

	Function createFunctionInvoker(
		query::Context&                    ctx,
		const std::vector<CRef<Function>>& functions,
		const base::StrID&                 mangled_name
	) {
		auto function_type = ctx.query<tsh::QueryFunctionType>({
			{},
			tsh::SymbolType{
				ctx.query<tsh::QueryUnitType>({}),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			},
		});

		auto return_type = ctx.query<tsl::QuerySymbolTypeLayout>(function_type.getResultType());

		Block entry_block;
		entry_block.terminator = Instruction{ Operation::ReturnVoid, {}, {} };

		for (const auto& function: functions) {
			entry_block.instructions.push_back(Instruction{
				Operation::Call,
				{},
				{ LIRValue{ FunctionLiteral::fromFunction(*function) } }

				,
			});
		}

		base::StableVector<Block> blocks;
		blocks.emplaceBack(std::move(entry_block));

		BlockRef entry_block_ref = blocks.last();

		return Function{
			.mangled_name       = mangled_name,
			.abi                = helios::DefaultAbi{},
			.return_type_layout = return_type,
			.parameter_layouts  = {},
			.blocks             = std::move(blocks),
			.local_list         = {},
			.block_order        = { entry_block_ref },
		};
	}

	FunctionLiteral getFunctionLiteralfromHELIOSID(query::Context& ctx, helios::SymID helios_id) {
		tsh::FunctionAbstractType type = ctx.query<helios::QueryTypeOfSymbol>(helios_id)
		                                     ->expect("Handling errors in MIR is not supported yet")
		                                     .getType();

		auto symbol_abi = ctx.query<helios::QuerySymbolABI>(helios_id)->expect(
			"Handling errors in MIR is not supported yet"
		);
		auto mangled_name = [&symbol_abi, &ctx, helios_id] {
			variant_match(symbol_abi) {
				variant_case(helios::DefaultAbi, _) {
					return helios::mangler::getSimpleMangledName(ctx, helios_id);
				}
				variant_case(helios::CAbi, _) { return helios::name(helios_id); }
			}
			CORE_UNREACHABLE();
		}();

		auto return_type = ctx.query<tsl::QuerySymbolTypeLayout>(type.getResultType());
		std::vector<tsl::TypeLayout> parameter_types;
		parameter_types.reserve(type.getParameterTypes().size());
		for (const auto& param: type.getParameterTypes())
			// Discard information-less parameters from LIR function parameter lists.
			if (param.getType().carriesInformation())
				parameter_types.push_back(ctx.query<tsl::QuerySymbolTypeLayout>(param));

		return FunctionLiteral{
			.mangled_name = mangled_name,
			.abi          = symbol_abi,
			.parameter_layouts
			= std::make_shared<std::vector<tsl::TypeLayout>>(std::move(parameter_types)),
			.return_type_layout = std::make_shared<tsl::TypeLayout>(std::move(return_type)),
		};
	}
}
