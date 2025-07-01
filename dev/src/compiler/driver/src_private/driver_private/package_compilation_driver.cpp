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

	// this is a quick hack, it will change with future driver refactor:
	// MRef<artifacts::ArtifactCollection> root_collection;

	// void setRootArtifactCollection(Ref<artifacts::ArtifactCollection> collection) {
	// 	CORE_ASSERT(root_collection == nullptr, "Root collection already set");
	// 	root_collection = collection;
	// }

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

	struct KeyOf_CompileModule final {
		frontend::ModuleID module_id;
		BackendType        backend_type;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return { module_id.asInt(), std::to_underlying(backend_type) };
		}
	};

	/**
	 * Query that produces QBC/.o file for given Duckling module.
	 */
	DECLARE_QUERY(CompileModule, KeyOf_CompileModule, artifacts::FileArtifact);

	struct IMPLEMENT_QUERY(CompileModule, artifacts::FileArtifact) {
		static Ref<artifacts::ArtifactCollection> getCollection() {
			// this should be automated in the future with some query component:
			return root_collection->subCollectionAtOrNew(base::StrID("query"))
			    ->subCollectionAtOrNew(
					base::StrID(base::strConcat("query", CompileModule::getID().asInt()).c_str())
				);
		}

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

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileModule);

	PackageCompilationDriver::PackageCompilationDriver(
		BackendType backend, fs::FilePath package_location, std::filesystem::path artifact_location
	):
		  backend{ backend },
		  package_location(std::move(package_location)),
		  root_artifact_collection(std::move(artifact_location)) {
		// setRootArtifactCollection(&root_artifact_collection);
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
