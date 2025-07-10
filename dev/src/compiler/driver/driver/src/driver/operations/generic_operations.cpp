#include "generic_operations.hpp"

#include <driver_private/operations.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/artifacts_location.hpp>
#include <helios/queries.hpp>
#include <linker/link.hpp>
#include <query_framework/query_artifacts_macros.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <utility>

namespace compiler::driver {
	base::Bit256 KeyOf_CompileModule::queryUnstablePerfectHash() const {
		return { module_id.asInt(), std::to_underlying(backend_type) };
	}

	struct IMPLEMENT_QUERY(CompileModule, artifacts::FileArtifact) {
		QUERY_ARTIFACTS_MACROS
		QUERY_AUTO_CACHE_COPY

		static auto provide(query::Context& ctx, QKey key) -> artifacts::FileArtifact {
			auto hout = ctx.query<helios::QueryModuleHOUT>(key.module_id);

			// Note: in the future it should use stable hashing for incremental
			// compilation. For now its ok.
			auto output_name = key.queryUnstablePerfectHash().toStringHex();

			auto output
				= getQueryArtifactsCollection()->fileArtifactAtOrNew(base::StrID(output_name.c_str()
			    ));
			auto module_name
				= base::StrID(base::strConcat("module_", key.module_id.asInt()).c_str());

			compileHOUTUnit(ctx, &hout, module_name, output, key.backend_type);

			return output;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileModule);

	void compilerEntirePackageIntoBinary(const fs::File& package_location, BackendType backend) {
		auto root = query::entryPoint<frontend::QueryModuleTree>(package_location);

		std::vector<artifacts::FileArtifact> objects;

		// this is std::function, so it can be recursive
		std::function<void(frontend::ModuleID)> handle_module
			= [&](frontend::ModuleID module_id) -> void {
			objects.emplace_back(query::entryPoint<CompileModule>({ module_id, backend }));
			auto sub_modules = query::entryPoint<frontend::QuerySubmodules>(module_id);
			for (const auto& [id, sub_module]: *sub_modules) handle_module(sub_module);
		};
		handle_module(root);

		if (backend == BackendType::LLVM) {
			// Link all outputs into a single binary.
			auto output_file = global_state::getRootCollection()->fileArtifactAtOrNew(
				base::StrID(base::strConcat("package_", backendTypeToStr(backend), ".exe").c_str())
			);

			objects.push_back(emitBuiltinLLVMObjectFile());
			link(output_file, objects, LinkOptions{ .link_c_standard_library = true });
		}
	}

}
