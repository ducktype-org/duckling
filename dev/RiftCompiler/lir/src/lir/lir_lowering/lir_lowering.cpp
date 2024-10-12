#include "lir_lowering.hpp"
#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/lower/queries.hpp>
#include <typesystem/higher/queries.hpp>

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
		static auto provide(Context& ctx, QKey key) -> PResult {
			// what we do here:
			// * map mir locals to lir locals
			// * go thru all blocks
			// * generate lir-blocks from each mir-block, by:
			//   * going thru all instructions
			//   * generating lir-instructions, lir-blocks from each mir-instruction
			// * mapping block numbers somehow

			// make locals:
			base::StableVector<LirLocal> locals;
			
			base::Map<mir::MirLocal, LocalRef> mir_to_lir_local;
			base::Map<mir::MirLocal, LocalRef> mir_to_lifetime_flag;
			
			for (const auto& mir_local : key.function.local_list) {
				auto lir_local = LirLocal::fromMir(ctx, *mir_local);
				auto lifetime_flag = LirLocal::boolLocal(ctx);

				auto pos = locals.pushBack(std::move(lir_local));
				auto flag_pos = locals.pushBack(std::move(lifetime_flag));

				mir_to_lir_local.put(mir_local.ref(), locals.getCRef(pos));
				mir_to_lifetime_flag.put(mir_local.ref(), locals.getCRef(flag_pos));
			}

			
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToLirFunction);
}

