#include "operations.hpp"

#include <query_framework/query_impl.hpp>
#include <query_framework/query_artifacts_macros.hpp>
#include <global_state/artifacts_location.hpp>

#include <utility>

namespace compiler::driver {
	base::Bit256 KeyOf_CompileModule::queryUnstablePerfectHash() const {
		return { module_id.asInt(), std::to_underlying(backend_type) };
	}

	struct IMPLEMENT_QUERY(CompileModule, artifacts::FileArtifact) {
		QUERY_ARTIFACTS_MACROS
		QUERY_AUTO_CACHE_COPY

		static auto provide(query::Context& ctx, QKey key) -> artifacts::FileArtifact {
			using namespace compiler;
			auto hout = ctx.query<helios::QueryModuleHOUT>(key.module_id);

			// here we create now backend driver per each query call,
			// which might be suboptimal
			auto binary_diver = HoutToBinaryDriver{ getBackendOptions(key.backend_type) };

			// Note: in the future it should use stable hashing for incremental
			// compilation. For now its ok.
			auto output_name = key.queryUnstablePerfectHash().toStringHex();

			auto output = getCollection()->fileArtifactAtOrNew(base::StrID(output_name.c_str()));
			auto module_name
				= base::StrID(base::strConcat("module_", key.module_id.asInt()).c_str());

			binary_diver.compileHOUTUnit(ctx, &hout, module_name, output);

			return output;
		}

	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileModule);
}
