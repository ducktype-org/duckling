#include "lir_lowering.hpp"
#include "../lir_structure/lir_structure.hpp"

#include <query_framework/query_impl.hpp>
#include <typesystem/lower/queries.hpp>

namespace compiler::lir {

	LirLocal MirLocal::fromMir(query::Context& ctx, const mir::MirLocal& mir_local) {
		auto type_layout = ctx.query<tsl::QueryTypeLayout>(mir_local.type.getType());

		return LirLocal{
			mir_local.helios_id,
			type_layout
		};
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

			
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToLirFunction);
}

