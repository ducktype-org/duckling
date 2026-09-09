#include "operations.hpp"

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/debug_artifacts.hpp>
#include <driver_private/lir_unit_with_name.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bit256.hpp>

#include <hashing/component_hash.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <fstream>

namespace compiler::driver {

	void LIRUnitWithBackendName::debugPrint(query::Context& ctx, std::ostream& os) const {
		os << "LIRUnitWithBackendName for module: " << module_id.strView() << "\n";
		lir_unit.debugPrint(os, Ref{ &ctx });
	}

	/**
	 * @brief Utility function to get an ofstream for dumping debug artifacts.
	 * The artifact will be created if it does not exist.
	 */
	std::ofstream getDebugDumpArtifact(base::StrID artifact_name) {
		auto          art = getDebugArtifactCollection()->fileArtifactAtOrNew(artifact_name);
		std::ofstream output_file(art.file.getFilePath().getPath(), std::ios::binary);
		return output_file;
	}

	query::QResult<LIRUnitWithBackendName> compileHOUTUnitToLIRModuleData(
		query::Context&         ctx,
		const helios::HOUTUnit& hout_unit,
		base::StrID             module_id,
		base::StrID             module_id_human
	) {
#define DEBUG_DUMP_ARTIFACT(str_id_name, ext) \
	getDebugDumpArtifact(base::StrID(base::strConcat(str_id_name.strView(), ext)))

		if (driver::print_ir_options.print_hir) hout_unit.debugPrint(ctx, std::cout);
		if (driver::dump_ir_options.dump_hir) {
			auto ofstream = DEBUG_DUMP_ARTIFACT(module_id_human, ".hir");
			hout_unit.debugPrint(ctx, ofstream);
		}

		auto mir_unit_qr = mir::lowerToMIRUnit(ctx, &hout_unit);
		if (mir_unit_qr.hasFailed()) return query::Failed();
		const auto& mir_unit = mir_unit_qr.valueOrPanic();

		if (driver::print_ir_options.print_mir) mir_unit.debugPrint(ctx, std::cout);
		if (driver::dump_ir_options.dump_mir) {
			auto ofstream = DEBUG_DUMP_ARTIFACT(module_id_human, ".mir");
			mir_unit.debugPrint(ctx, ofstream);
		}

		auto lir_module = LIRUnitWithBackendName{
			.module_id       = module_id,
			.module_id_human = module_id_human,
			.lir_unit        = lir::lowerToLIRUnit(ctx, mir_unit),
		};

		if (driver::print_ir_options.print_lir) lir_module.debugPrint(ctx, std::cout);
		if (driver::dump_ir_options.dump_lir) {
			auto ofstream = DEBUG_DUMP_ARTIFACT(module_id_human, ".lir");
			lir_module.debugPrint(ctx, ofstream);
		}

		return lir_module;
	}

	query::QResult<LIRUnitWithBackendName> compileModuleToLIRModuleData(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		const auto& hout_unit_qr = ctx.query<helios::QueryModuleHOUT>(module_id);
		if (hout_unit_qr->hasFailed()) return query::Failed();

		auto module_str_id   = base::StrID(base::strConcat(
            "module_",
            compiler::frontend::ModuleTree::getPathComponentHash(module_id).hash.toStringHex()
        ));
		auto module_id_human = base::StrID(frontend::getModuleRef(module_id)->humanReadableID(ctx));

		return compileHOUTUnitToLIRModuleData(
			ctx, hout_unit_qr->valueOrPanic(), module_str_id, module_id_human
		);
	}
}
