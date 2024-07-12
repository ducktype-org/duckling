#include "mir_lowering.hpp"
#include <query_framework/query_impl.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>


namespace compiler::mir {


	// @TODO: HoleID, BlockID, 

	struct StmtBlockVisitor: public helios::code::HoutStmtVisitor {
		// ...
	};

	struct StmtExprVisitor: public helios::code::HoutExprVisitor {
		// ...
	};

	// @TODO: StmtExprBoolJmpVisitor for jumping code

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