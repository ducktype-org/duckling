/**
 * @file generic_operations.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "generic_operations.hpp"

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/backend_operations/compile_dvm.hpp>
#include <driver_private/backend_operations/compile_llvm.hpp>
#include <driver_private/operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <linker/link.hpp>
#include <time_stats/time_stats.hpp>

#include <base/collections/optional.hpp>
#include <base/types/ok_bad.hpp>

#include <hashing/component_hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/standard_query/query_artifacts_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <fstream>
#include <chrono>
#include <iostream>
#include <utility>
#include <vector>

namespace compiler::driver {
	base::Bit256 KeyOf_CompileModule::queryUnstablePerfectHash() const {
		return { module_id.queryUnstablePerfectHash(), std::to_underlying(backend_type) };
	}

	base::Bit256 KeyOf_CompileModule::queryStablePerfectHash() const {
		auto component_hash = compiler::frontend::ModuleTree::getPathComponentHash(module_id);
		auto partial        = component_hash.partial;
		hashing::addToHash(partial, std::to_underlying(backend_type));
		return partial.finalize();
	}

	struct IMPLEMENT_QUERY(CompileModule, query::QResult<artifacts::FileArtifact>) {
		QUERY_ARTIFACTS_MACROS
		QUERY_AUTO_CACHE_COPY

		/**
		 * Helper function to get full module name for logging purposes.
		 */
		static std::string getModuleFullName(frontend::ModuleID module_id) {
			std::string out;
			if (getModuleRef(module_id)->getParentModule().has_value()) {
				out += getModuleFullName(
					getModuleRef(module_id)->getParentModule().value().illegalAccess().getID()
				);
				out += "/";
			}
			out += getModuleRef(module_id)->getName().strView();
			return out;
		}

		/**
		 * Helper function to log module compilation info.
		 */
		static void moduleLog(const QKey& key, std::string_view info) {
			CORE_USER_LOG(
				"[?/?] Compiling ",
				getModuleFullName(key.module_id),
				" (",
				backendTypeToStr(key.backend_type),
				")",
				": ",
				info,
				"!\n"
			);
		}

		static auto typeExtension(BackendType backend) {
			switch (backend) {
			case BackendType::LLVM:
				return ".o";
			case BackendType::DVM:
				return ".dbc";
			default:
				CORE_PANIC("bad backend type");
			}
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			moduleLog(key, "Recompiling");

			auto lir_data_result = ctx.query<CompileToLIRModuleData>(key.module_id);
			if (lir_data_result->hasFailed()) {
				moduleLog(key, "Compilation failed");
				return query::Failed();
			}
			CRef lir_data = &lir_data_result->valueOrThrow();

			auto output_name
				= key.queryStablePerfectHash().toStringHex() + typeExtension(key.backend_type);
			auto output
				= getQueryArtifactsCollection()->fileArtifactAtOrNew(base::StrID(output_name.c_str()
			    ));

			switch (key.backend_type) {
			case BackendType::LLVM: {
				auto llvm_module = compileLIRModuleToLLVM(ctx, lir_data);
				{
					// compileLIRModuleToLLVM time is added on its own,
					// but tracking time of the actual compilation to object file is done here

					llvm_module.compile(
						output.file.getFilePath(), backend_llvm::CompilationOutputType::Object
					);
				}

				if (driver::llvm_dump_ir) {
					base::StrID llvm_ir_path
						= base::StrID(base::strConcat(lir_data->module_id.strView(), ".ll").c_str());
					llvm_module.dumpLLVMToFile(llvm_ir_path);
				}
				if (driver::llvm_dump_asm) {
					base::StrID assembly_path
						= base::StrID(base::strConcat(lir_data->module_id.strView(), ".s").c_str());
					llvm_module.compile(
						assembly_path.strView(), backend_llvm::CompilationOutputType::Assembly
					);
				}
				break;
			}
			case BackendType::DVM: {
				auto          dvm_code_collection = compileLIRModuleToDVM(lir_data);
				std::ofstream dvm_file(output.file.getFilePath().getPath(), std::ios::binary);
				if (!dvm_file.is_open()) CORE_PANIC("Failed to open DVM file for writing");
				vm::code::serialize(dvm_code_collection, dvm_file);
				dvm_file.close();
				break;
			}
			default:
				CORE_PANIC("bad backend type");
			}

			return output;
		}

		/**
		 * Load precompiled artifact from disk without performing any compilation.
		 * Returns Optional empty if the underlying file does not exist anymore.
		 */
		static auto loadFromDisc(const QKey& key) -> base::Optional<PResult> {
			auto output_name
				= key.queryStablePerfectHash().toStringHex() + typeExtension(key.backend_type);

			auto collection   = getQueryArtifactsCollection();
			auto output_maybe = collection->fileArtifactAtMaybe(base::StrID(output_name.c_str()));

			if (!output_maybe.has_value()) {
				moduleLog(key, "artifact not found in artifacts collection");
				return {};
			}

			auto output = *output_maybe.value();

			moduleLog(key, "Cached, loading artifact from disk");
			return output;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileModule);

	base::OkBad compileEntirePackage(
		const global_state::PackageInfo& package_info,
		BackendType                      backend,
		const linker::LinkingOptions&    linking_options
	) {
		auto        root   = package_info.root_module;
		// base::OkBad result = base::OK;

		std::vector<artifacts::FileArtifact> objects;

		std::vector<frontend::ModuleID> modules_to_compile;

		std::function<void(frontend::ModuleID)> collect_modules
			= [&](frontend::ModuleID module_id) -> void {
				modules_to_compile.push_back(module_id);
				auto sub_modules = query::entryPoint<frontend::QuerySubmodules>(module_id);
				for (const auto& [id, sub_module]: *sub_modules) collect_modules(sub_module);
			};
		collect_modules(root);

		std::atomic_flag modules_failed;
		modules_failed.clear();

		auto compile_t0 = std::chrono::steady_clock::now();

		query::utils::withContextDo([&](query::Context& ctx) {
			std::vector<query::internal::TaskHandle> handles;
			for (const auto& module_id: modules_to_compile) {
				handles.push_back(ctx.schedule<CompileModule>({ module_id, backend }));
			}
			for (auto& handle: handles) {
				auto module_result = ctx.await<CompileModule>(handle);
				if (module_result.hasValue())
					objects.emplace_back(module_result.valueOrPanic());
				else
					modules_failed.test_and_set();
			}
		});

		auto compile_t1 = std::chrono::steady_clock::now();
		auto compile_us = std::chrono::duration_cast<std::chrono::microseconds>(compile_t1 - compile_t0).count();
		fprintf(stderr, "\n  Total multi-module compilation time: %ld us (%zu modules)\n\n", compile_us, modules_to_compile.size());

		if (modules_failed.test()) return base::BAD;

		if (backend == BackendType::LLVM) {
			// Link all outputs into a single binary.
			auto output_file = global_state::getRootCollection()->fileArtifactAtOrNew(
				base::StrID(base::strConcat("package_", backendTypeToStr(backend), ".exe").c_str())
			);

			objects.push_back(emitBuiltinLLVMObjectFile());

			linker::link(output_file, objects, linking_options);
		}

		return base::OK;
	}

	std::expected<RunOutput, std::string> runModuleOnDVM(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		CRef lir_data            = &ctx.query<CompileToLIRModuleData>(module_id)->valueOrPanic();
		auto dvm_code_collection = compileLIRModuleToDVM(lir_data);

		vm::PID pid{};

		return vm::api::spawn()
		    .and_then([&](vm::api::ProcessInfo process) {
				pid = process.pid;
				return std::expected<void, vm::api::ApiError>{};
			})
		    .and_then([&] { return vm::api::loadCode(pid, { dvm_code_collection }); })
		    .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
		    .and_then([&] { return vm::api::run(pid); })
		    .and_then([&] { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .transform([](Ref<vm::VmValue> exit_value) {
				return RunOutput{ .exit_code
				                  = base::safeIntConv<int>(exit_value->readBytes<i64>()) };
			});
	}
}
