#include "lir_lowering.hpp"
#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/lower/queries.hpp>
#include <typesystem/higher/queries.hpp>

namespace compiler::lir {

	base::HashT KeyOf_LowerToLirFunction::customPerfectHash() const {
		return base::perfectHash(function);
	}

	LirLocal LirLocal::fromMir(query::Context& ctx, const mir::MirLocal& mir_local) {
		auto type_layout = ctx.query<tsl::QueryTypeLayout>(mir_local.type.getType());

		return LirLocal{
			mir_local.helios_id,
			type_layout
		};
	}

	LirLocal LirLocal::boolLocal(query::Context& ctx) {
		auto bool_type = ctx.query<tsh::QueryBoolType>({});
		auto bool_layout = ctx.query<tsl::QueryTypeLayout>(bool_type);
		
		return LirLocal{bool_layout};
	}



	struct IMPLEMENT_QUERY(LowerToLirFunction, Function) {

		/**
		 * @brief Helper struct for easy state encapsulation used.
		 * by LowerToLirFunction query. 
		 * @note This is not a typical type, and
		 * should be seen as a set of functions operating on some common state.
		 * Order of those functions matter, as they build LIR state
		 * step by step.
		 */
		struct Mir2Lir {
			Context& ctx;
			QKey key;

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

			LocalRef getLocal(mir::LocalRef mir_local) {
				return mir_to_lir_local.at(mir_local);
			}

			LirLocation getLocation(const mir::MirLocation& loc) {
				variant_match(loc.getVariant()) {
					variant_case(mir::MirIntegerConst, integer) {
						return LirLocation{integer.value};
					}
					variant_case(mir::LocalRef, local) {
						return LirLocation{getLocal(local)};
					}
					variant_case(mir::BlockID, block) {
						return LirLocation{BlockRef(mir_to_lir_block.at(block))};
					}
				}
				CORE_PANIC("Unhandled variant in getLocation");
			}

			// main functions:
			
			void makeLocals() {
				for (const auto& mir_local : key.function.local_list) {
					auto lir_local = LirLocal::fromMir(ctx, *mir_local);
					auto lifetime_flag = LirLocal::boolLocal(ctx);

					auto pos = locals.pushBack(std::move(lir_local));
					auto flag_pos = locals.pushBack(std::move(lifetime_flag));

					mir_to_lir_local.put(mir_local.ref(), locals.getCRef(pos).value());
					mir_to_lifetime_flag.put(mir_local.ref(), locals.getCRef(flag_pos).value());
				}
			}

			void makeInitialBlocks() {
				// make initial block mapping, and
				// unfilled blocks that will map to 
				// beginning of each mir block
				for (const auto& mir_block : key.function.blocks) {
					// note that this block will only be filled with instructions
					// and terminator later:
					auto pos = blocks.pushBack({});
					mir_to_lir_block.put(mir_block.id, blocks.getRef(pos).value());
				}	
			}

			void lowerBlocks() {
				for (const auto& block: key.function.blocks) {
					const auto lir_block = mir_to_lir_block[block.id];
					block_order.emplace_back(lir_block);

					auto curr_block = lir_block;
					for (const auto& mir_instruction : block.instructions) {
						curr_block = lowerInstruction(curr_block, mir_instruction);
					}

					lowerTerminator(curr_block, block.terminator);
				}
			}

			// instruction lowering functions:

			std::vector<LirLocation> getLocations(const std::vector<mir::MirLocation>& locs) {
				std::vector<LirLocation> result;
				result.reserve(locs.size());
				for (const auto& loc : locs) {
					result.push_back(getLocation(loc));
				}
				return result;
			}

			void lowerFlags(MutBlockRef curr_block, const mir::Instruction& mir_instruction) {
				// @TODO
				// this does not produce new blocks?
				for (const auto& flag : mir_instruction.flags) {
					auto lir_local = getLocal(flag.local);
					switch (flag.flag) {
						using enum mir::OperationFlag::Flag;
						case Construct:
							// @TODO -- set lifetime flag
						case Destruct:
							// @TODO -- ??? (also: after or before...?)
						case Move:
							// @TODO -- unset lifetime flag
						default:
							throw base::NotYetImplemented("flag in lowerFlags");
					}
				}
			}

			LirOperation mir2lirOperation(mir::Operation mir_operation) {
				switch (mir_operation) {
					case mir::Operation::Assign:
						return LirOperation::Assign;
					case mir::Operation::ReturnValue:
						return LirOperation::ReturnValue;
					case mir::Operation::ReturnVoid:
						return LirOperation::ReturnVoid;
					// @TODO: add more cases
					default:
						CORE_PANIC("Operation without direct counterpart");
				}
			}

			/**
			 * @brief Lowers instruction from MIR to LIR.
			 * * Fills @p curr_block.
			 * Legal to use only in lowerBlocks
			 * @param curr_block 
			 * @return next curr_block
			 */
			MutBlockRef lowerInstruction(MutBlockRef curr_block, const mir::Instruction& mir_instruction) {
				// curr_block already in order
				
				CORE_ASSERT(not mir::isTerminating(mir_instruction.operation), "Terminator in lowerInstruction");
				lowerFlags(curr_block, mir_instruction);

				switch (mir_instruction.operation) {
					case mir::Operation::Assign:
					// here much more cases will be added 
					{
						auto output = getLocal(mir_instruction.output.value());
						auto args = getLocations(mir_instruction.arguments);
						curr_block->instructions.emplace_back(
							mir2lirOperation(mir_instruction.operation),
							output,
							std::move(args)
						);
						return curr_block;
					}
					case mir::Operation::DestructIf:
						// @TODO implement it, once we know how to call destructors 
						std::cerr << "DestructIf not implemented in LIR, skipping" << "\n";
						return curr_block;
					default:
						throw base::NotYetImplemented("instruction in LowerToLirFunction");
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
				// curr_block alfredy in order
				CORE_ASSERT(mir::isTerminating(mir_terminator.operation), "non-Terminator in lowerTerminator");
				lowerFlags(curr_block, mir_terminator);
				
				switch (mir_terminator.operation) {
					case mir::Operation::ReturnValue:
					case mir::Operation::ReturnVoid: {
						CORE_ASSERT(mir_terminator.output.empty(), "terminator should not return");
						auto args = getLocations(mir_terminator.arguments);
						curr_block->terminator = Instruction{
							mir2lirOperation(mir_terminator.operation),
							{},
							std::move(args)
						};
						break;
					}
					// @TODO: add more cases
					default:
						throw base::NotYetImplemented("terminator in LowerToLirFunction");
				}
			}

			Function get() {
				return Function{
					key.function.name,
					std::move(blocks),
					std::move(locals),
					std::move(block_order)
				};
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			// LirFunction is build-inplace here, and for this reason
			// actives legal state only at the end.
			// for this reason additional assertions should be put inplace, to 
			// ensure that the state is legal.

			// what we do here:
			// * map mir locals to lir locals
			// * go thru all blocks
			// * generate lir-blocks from each mir-block, by:
			//   * going thru all instructions
			//   * generating lir-instructions, lir-blocks from each mir-instruction
			// * mapping block numbers somehow

			Mir2Lir mir2lir{ctx, key};

			// call order matters:
			mir2lir.makeLocals();
			mir2lir.makeInitialBlocks();
			mir2lir.lowerBlocks();

			return mir2lir.get();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToLirFunction);
}

