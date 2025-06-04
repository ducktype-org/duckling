#include "package_compilation_driver.hpp"

#include "backend_driver/llvm_ir_lib.hpp"
#include "link.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <driver/hout_to_binary_driver.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::driver {

	// this is a quick hack, change it in this PR:
	MRef<artifacts::ArtifactCollection> main_collection;

	BackendOptions getBackendOptions(BackendType type) {
		if (type == BackendType::LLVM) {
			return BackendOptions{
				.backend_type           = type,
				.compile_to_assembly    = false,
				.dump_llvm_ir           = false,
				.dvm_code_only_memory   = false,
				.add_builtin_library    = true,
				.external_objects_files = {},
				.external_libs          = {},
			};
		} else if (type == BackendType::DVM) {
			return BackendOptions{
				.backend_type           = type,
				.compile_to_assembly    = false,
				.dump_llvm_ir           = false,
				.dvm_code_only_memory   = false,
				.add_builtin_library    = true,
				.external_objects_files = {},
				.external_libs          = {},
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

	DECLARE_QUERY(CompilerModuleToLLVM, frontend::ModuleID, artifacts::FileArtifact);

	struct IMPLEMENT_QUERY(CompilerModuleToLLVM, artifacts::FileArtifact) {
		static Ref<artifacts::ArtifactCollection> getCollection() {
			// this is far from pretty:
			return main_collection->subCollectionAtOrNew(base::StrID("query"))
			    ->subCollectionAtOrNew(base::StrID(
					base::strConcat("query", CompilerModuleToLLVM::getID().asInt()).c_str()
				));
		}

		static auto provide(query::Context& ctx, frontend::ModuleID module_id)
			-> artifacts::FileArtifact {
			using namespace compiler;
			auto hout         = ctx.query<helios::QueryModuleHOUT>(module_id);
			auto binary_diver = HoutToBinaryDriver{ getBackendOptions(BackendType::LLVM) };

			auto output = getCollection()->fileArtifactAtOrNew(
				base::StrID(base::strConcat("module_", module_id.asInt(), ".o").c_str())
			);
			auto module_name = base::StrID(base::strConcat("module_", module_id.asInt()).c_str());

			binary_diver.compileHOUTUnit(ctx, &hout, module_name, output);

			return output;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompilerModuleToLLVM);

	PackageCompilationDriver::PackageCompilationDriver(
		BackendType backend, fs::FilePath package_location, std::filesystem::path artifact_location
	):
		  backend{ backend },
		  package_location(std::move(package_location)),
		  root_artifact_collection(std::move(artifact_location)) {
		main_collection = &root_artifact_collection;
	}

	void PackageCompilationDriver::compilerEntirePackageIntoBinary() {
		using namespace compiler;
		auto root = query::entryPoint<frontend::QueryModuleTree>(package_location);

		std::vector<artifacts::FileArtifact> objects;

		std::function<void(frontend::ModuleID)> handle_module
			= [&](frontend::ModuleID module_id) -> void {
			objects.emplace_back(query::entryPoint<CompilerModuleToLLVM>(module_id));
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
			link(
				output_file,
				objects,
				compiler::driver::LinkOptions{ .link_c_standard_library = true }
			);
		}
	}

	artifacts::FileArtifact PackageCompilationDriver::emitBuiltinObjectFile() {
		auto builtin_obj_file = root_artifact_collection.fileArtifactAtOrNew(
			base::StrID(base::strConcat("builtin_", backendTypeToStr(backend), ".o").c_str())
		);
		auto mod = backend_llvm::Module::fromIRCode(LLVM_IR_LIB);
		mod.compile(builtin_obj_file.FILE, backend_llvm::CompilationOutputType::Object);
		return builtin_obj_file;
	}
}
