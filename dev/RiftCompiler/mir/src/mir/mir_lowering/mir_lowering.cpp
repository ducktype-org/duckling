#include "mir_lowering.hpp"
#include <query_framework/query_impl.hpp>

namespace compiler::mir {

	struct IMPLEMENT_QUERY(LowerToMirFunction, Function) {
		auto provide(Context& ctx, QKey key) {
			// @TODO:
			// * build cfg+quad step by step
			// * add some lifetime stuff
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToMirFunction);



}