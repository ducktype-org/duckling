#include "lir_lowering.hpp"
#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/lower/queries.hpp>
#include <typesystem/higher/queries.hpp>

#include <algorithm>

namespace compiler::lir {

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
			base::Map<mir::MirLocal, LocalRef> mir_to_lir_local;
			base::Map<mir::MirLocal, LocalRef> mir_to_lifetime_flag;
			
			base::StableVector<Block> blocks;
			
			// blocks mapping:
			base::Map<mir::BlockID, MutBlockRef> mir_to_lir_block;

			std::vector<BlockRef> block_order;

			void makeLocals() {
				for (const auto& mir_local : key.function.local_list) {
					auto lir_local = LirLocal::fromMir(ctx, *mir_local);
					auto lifetime_flag = LirLocal::boolLocal(ctx);

					auto pos = locals.pushBack(std::move(lir_local));
					auto flag_pos = locals.pushBack(std::move(lifetime_flag));

					mir_to_lir_local.put(mir_local.ref(), locals.getCRef(pos));
					mir_to_lifetime_flag.put(mir_local.ref(), locals.getCRef(flag_pos));
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
					mir_to_lir_block.put(mir_block.id, blocks.getRef(pos));
				}	
			}

			void lowerBlocks() {
				for (const auto& block: key.function.blocks | std::views::reverse) {
					auto lir_block = mir_to_lir_block[block.id];
			
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

			// call order here matters:
			mir2lir.makeLocals();
			mir2lir.makeInitialBlocks();
			mir2lir.lowerBlocks();
			
			return mir2lir.get();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToLirFunction);
}

