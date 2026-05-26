#include "operations.hpp"

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/debug_artifacts.hpp>
#include <driver_private/lir_module_data.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <hashing/component_hash.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

// #include <algorithm>
#include <fstream>

namespace compiler::driver {
	

	void LIRUnitWithBackendName::debugPrint(query::Context& ctx, std::ostream& os) const {
		os << "LIRUnitWithBackendName for module: " << module_id.strView() << "\n";
		lir_unit.debugPrint(ctx, os);
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

	struct IMPLEMENT_QUERY(CompileHOUTUnitToLIRModuleData, query::QResult<LIRUnitWithBackendName>) {
		static auto provide(query::Context& ctx, CompileHOUTUnitToLIRModuleDataKey key) -> PResult {
			const auto& hout_unit   = *key.hout_unit.get();
			auto        module_name = key.module_name;

			if (driver::print_ir_options.print_hir) hout_unit.debugPrint(ctx, std::cout);
			if (driver::dump_ir_options.dump_hir) {
				auto ofstream = getDebugDumpArtifact(
					base::StrID(base::strConcat(module_name.strView(), ".hir"))
				);
				hout_unit.debugPrint(ctx, ofstream);
			}

			mir::MIRUnit mir_unit = mir::lowerToMIRUnit(ctx, &hout_unit).valueOrThrow();

			if (driver::print_ir_options.print_mir) {
				mir_unit.debugPrint(ctx, std::cout);
			}
			if (driver::dump_ir_options.dump_mir) {
				auto ofstream = getDebugDumpArtifact(
					base::StrID(base::strConcat(module_name.strView(), ".mir"))
				);
				mir_unit.debugPrint(ctx, ofstream);
			}

			auto lir_module = LIRUnitWithBackendName{
				.module_id = module_name,
				.lir_unit  = lir::lowerToLIRUnit(ctx, mir_unit),
			};

			if (driver::print_ir_options.print_lir) lir_module.debugPrint(ctx, std::cout);
			if (driver::dump_ir_options.dump_lir) {
				auto ofstream = getDebugDumpArtifact(
					base::StrID(base::strConcat(module_name.strView(), ".lir"))
				);
				lir_module.debugPrint(ctx, ofstream);
			}

			return lir_module;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileHOUTUnitToLIRModuleData);

	struct IMPLEMENT_QUERY(CompileToLIRModuleData, query::QResult<LIRUnitWithBackendName>) {
		QUERY_AUTO_CACHE_CREF

		// #2246 this should go
		static auto provide(query::Context& ctx, frontend::ModuleID module_id) -> PResult {
			const auto& hout_unit = ctx.query<helios::QueryModuleHOUT>(module_id)->valueOrThrow();

			auto module_name = base::StrID(base::strConcat(
				"module_",
				compiler::frontend::ModuleTree::getPathComponentHash(module_id).hash.toStringHex()
			));

			// we intentially make copy here, to keep the data in the
			// cache of this query
			return *ctx.query<CompileHOUTUnitToLIRModuleData>({ &hout_unit, module_name });
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileToLIRModuleData);
}
