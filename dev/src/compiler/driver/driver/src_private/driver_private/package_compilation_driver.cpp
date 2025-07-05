#include "package_compilation_driver.hpp"

#include "backend_driver/llvm_ir_lib.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <driver/hout_to_binary_driver.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::driver {

	BackendOptions getBackendOptions(BackendType type) {
		if (type == BackendType::LLVM) {
			return BackendOptions{
				.backend_type         = type,
				.compile_to_assembly  = false,
				.dump_llvm_ir         = false,
				.dvm_code_only_memory = false,
				.add_builtin_library  = true,
			};
		} else if (type == BackendType::DVM) {
			return BackendOptions{
				.backend_type         = type,
				.compile_to_assembly  = false,
				.dump_llvm_ir         = false,
				.dvm_code_only_memory = false,
				.add_builtin_library  = true,
			};
		} else {
			CORE_UNREACHABLE();
		}
	}

	std::string backendTypeToStr(BackendType type) {
		switch (type) {
		case BackendType::LLVM:
			return "llvm";
		case BackendType::DVM:
			return "dvm";
		default:
			CORE_UNREACHABLE();
		}
	}



	void PackageCompilationDriver::compilerEntirePackageIntoBinary() {
		using namespace compiler;
		auto root = query::entryPoint<frontend::QueryModuleTree>(package_location);

		std::vector<artifacts::FileArtifact> objects;

		// this is std::function, so it can be recursive
		std::function<void(frontend::ModuleID)> handle_module
			= [&](frontend::ModuleID module_id) -> void {
			objects.emplace_back(query::entryPoint<CompileModule>({ module_id, this->backend }));
			auto sub_modules = query::entryPoint<frontend::QuerySubmodules>(module_id);
			for (const auto& [id, sub_module]: *sub_modules) handle_module(sub_module);
		};
		handle_module(root);

		if (backend == BackendType::LLVM) {
			// Link all outputs into a single binary.
			auto output_file = root_artifact_collection.fileArtifactAtOrNew(
				base::StrID(base::strConcat("package_", backendTypeToStr(backend), ".exe").c_str())
			);

			objects.push_back(emitBuiltinObjectFile());
			link(output_file, objects, LinkOptions{ .link_c_standard_library = true });
		}
	}


}
