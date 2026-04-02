/**
 * @file generic_operations.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "generic_operations.hpp"

#include <debug_info/debug_info_io.hpp>
#include <driver/debug_info/debug_info.hpp>
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
#include <helios/queries/queries.hpp>
#include <linker/link.hpp>
#include <time_stats/time_stats.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/types/ok_bad.hpp>

#include <hashing/component_hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/standard_query/query_artifacts_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <fstream>
#include <iostream>
#include <utility>

namespace compiler::driver {
	base::Bit256 KeyOf_CompileModule::queryUnstablePerfectHash() const {
		return { module_id.queryUnstablePerfectHash(),
			     std::to_underlying(backend_type),
			     build_debug_info };
	}

	base::Bit256 KeyOf_CompileModule::queryStablePerfectHash() const {
		auto component_hash = compiler::frontend::ModuleTree::getPathComponentHash(module_id);
		auto partial        = component_hash.partial;
		hashing::addToHash(partial, std::to_underlying(backend_type));
		hashing::addToHash(partial, build_debug_info);
		return partial.finalize();
	}

	struct IMPLEMENT_QUERY(CompileModule, query::QResult<CompileModuleResult>) {
		QUERY_ARTIFACTS_MACROS
		QUERY_AUTO_CACHE_CREF

		/** Helper variable for printing user logs, change freely if needed */
		constinit static inline std::atomic<u64> total_module_count = 0;

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
			/** Helper variables for printing user logs, change freely if needed */
			static concurrent::ConHashMap<frontend::ModuleID, u64> module_number_cache;
			static std::atomic<u64>                                next_module_number = 1;

			u64 id = 0;
			module_number_cache.maybePutAndUpdate(key.module_id, u64{ 0 }, [&](Ref<u64> number) {
				if (*number == 0) {
					// this is a new module
					*number = next_module_number.fetch_add(1, std::memory_order_relaxed);
				}
				id = *number;
			});

			auto total = total_module_count.load(std::memory_order_relaxed);
			CORE_USER_LOG(
				"[",
				id,
				"/",
				total == 0 ? "?" : std::to_string(total),
				"] ",
				" Compiling ",
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

		struct ModuleOutputNames {
			std::string                 object_file;
			base::Optional<std::string> debug_info_file;
		};

		static ModuleOutputNames getModuleOutputName(const QKey& key) {
			ModuleOutputNames names;
			names.object_file
				= key.queryStablePerfectHash().toStringHex() + typeExtension(key.backend_type);
			if (key.build_debug_info && key.backend_type == BackendType::DVM) {
				names.debug_info_file
					= key.queryStablePerfectHash().toStringHex().append(DEBUG_INFO_STABLE_EXTENSION);
			}
			return names;
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			moduleLog(key, "Recompiling");

			auto lir_data_result = ctx.query<CompileToLIRModuleData>(key.module_id);
			if (lir_data_result->hasFailed()) {
				moduleLog(key, "Compilation failed");
				return query::Failed();
			}
			CRef lir_data = &lir_data_result->valueOrThrow();

			auto output_names = getModuleOutputName(key);
			auto code_output  = getQueryArtifactsCollection()->fileArtifactAtOrNew(
                base::StrID(output_names.object_file.c_str())
            );

			base::Optional<debug_info::DebugInfo>   debug_info_output;
			base::Optional<artifacts::FileArtifact> debug_info_artifact;

			switch (key.backend_type) {
			case BackendType::LLVM: {
				auto llvm_module = compileLIRModuleToLLVM(ctx, lir_data);
				{
					// compileLIRModuleToLLVM time is added on its own,
					// but tracking time of the actual compilation to object file is done here
					time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

					llvm_module.compile(
						code_output.file.getFilePath(), backend_llvm::CompilationOutputType::Object
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
				if_opt_some(output_names.debug_info_file, di_file) {
					debug_info_artifact.emplace(getQueryArtifactsCollection()->fileArtifactAtOrNew(
						base::StrID(di_file.c_str())
					));
				}

				auto serialize_to_artifact = [&](artifacts::FileArtifact& art,
				                                 auto&                    source,
				                                 auto                     serialize_fn) {
					std::ofstream output_file(art.file.getFilePath().getPath(), std::ios::binary);
					if (!output_file.is_open()) CORE_PANIC("Failed to open file for writing");
					serialize_fn(source, output_file);
					output_file.close();
				};

				auto dvm_module_data = compileLIRModuleToDVM(lir_data, ctx, key.build_debug_info);

				serialize_to_artifact(code_output, dvm_module_data.code, vm::code::serializeCode);

				if (key.build_debug_info) {
					dvm_module_data.debug_info.value().module_path
						= code_output.file.getFilePath().string();

					serialize_to_artifact(
						debug_info_artifact.value(),
						dvm_module_data.debug_info.value(),
						debug_info::saveToStream
					);

					debug_info_output = std::move(dvm_module_data.debug_info);
				}

				break;
			}
			default:
				CORE_PANIC("bad backend type");
			}

			return CompileModuleResult{ .object_art = std::move(code_output),
				                        .debug_info = std::move(debug_info_output) };
		}

		/**
		 * Load precompiled artifact from disk without performing any compilation.
		 * Returns Optional empty if the underlying file does not exist anymore.
		 */
		static auto loadFromDisc(const QKey& key) -> base::Optional<PResult> {
			auto output_name = getModuleOutputName(key);

			auto collection = getQueryArtifactsCollection();
			auto output_maybe
				= collection->fileArtifactAtMaybe(base::StrID(output_name.object_file.c_str()));

			if (!output_maybe.has_value()) {
				moduleLog(key, "artifact not found in artifacts collection");
				return {};
			}


			CompileModuleResult result{ .object_art = *output_maybe.value(), .debug_info = {} };

			if_opt_some(output_name.debug_info_file, di_file) {
				auto debug_info_maybe
					= collection->fileArtifactAtMaybe(base::StrID(di_file.c_str()));

				if (!debug_info_maybe.has_value()) {
					moduleLog(key, "debug info artifact not found in artifacts collection");
					return {};
				}

				std::ifstream input_file(
					debug_info_maybe.value()->file.getFilePath().getPath(), std::ios::binary
				);
				if (!input_file.is_open()) {
					moduleLog(key, "Failed to open debug info artifact file");
					return {};
				}

				auto debug_info_or_error = debug_info::loadFromStream(input_file);
				if (!debug_info_or_error.has_value()) {
					CORE_USER_LOG(
						"Failed to parse debug-info artifact: ", debug_info_or_error.error(), "\n"
					);
					return {};
				}
				result.debug_info.emplace(std::move(debug_info_or_error.value()));
			}

			moduleLog(key, "Cached, loading artifact from disk");
			return result;
		}

		static auto deleteFromDisc(const QKey& key) -> bool {
			auto output_name = getModuleOutputName(key);

			auto collection = getQueryArtifactsCollection();
			moduleLog(key, "Deleting artifact from disk");
			bool deleted
				= collection->deleteFileArtifact(base::StrID(output_name.object_file.c_str()));
			if_opt_some(output_name.debug_info_file, di_file) {
				bool artifact_deleted
					= collection->deleteFileArtifact(base::StrID(di_file.c_str()));
				deleted = deleted && artifact_deleted;
			}
			return deleted;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileModule);

	base::OkBad compileScript(
		[[maybe_unused]] const CompilerModeOfOperationAndOptions::ScriptMode& mode,
		[[maybe_unused]] BackendType                                          backend_type,
		[[maybe_unused]] const linker::LinkingOptions&                        linking_options
	) {
		// Steps:
		// 1. Read mode.script_file content
		// 2. Call repl::splitInputIntoStatements() to split into individual statement strings
		// 3. For each statement: create a chained REPL module (ReplData with parent link)
		// 4. For each module: compile and collect .dbc/.o artifacts
		// 5. DVM:  merge CodeCollections and serialize to mode.output_path as .dbc
		//    LLVM: compile entry-point module + link all .o files somehow (not yet sure how)
		throw base::NotYetImplemented("compileScript");
	}

	base::OkBad compileEntirePackage(
		const global_state::PackageInfo& package_info,
		BackendType                      backend,
		const linker::LinkingOptions&    linking_options
	) {
		auto        root   = package_info.root_module;
		base::OkBad result = base::OK;

		std::vector<artifacts::FileArtifact> objects;

		std::vector<frontend::ModuleID> modules_to_compile;

		std::function<void(frontend::ModuleID)> collect_modules
			= [&](frontend::ModuleID module_id) -> void {
			modules_to_compile.push_back(module_id);
			auto sub_modules = query::entryPoint<frontend::QuerySubmodules>(module_id);
			for (const auto& [id, sub_module]: *sub_modules) collect_modules(sub_module);
		};
		collect_modules(root);

		ImplementationOf_CompileModule::total_module_count.store(modules_to_compile.size());

		// @TODO: #2354 This is temporary.
		const bool build_debug_info = backend == BackendType::DVM;

		std::function<void(frontend::ModuleID)> handle_module
			= [&](frontend::ModuleID module_id) -> void {
			auto module_result
				= query::entryPoint<CompileModule>({ module_id, backend, build_debug_info });
			if (module_result->hasValue()) {
				objects.emplace_back(module_result->valueOrPanic().object_art);
				if (build_debug_info) query::entryPoint<DebugInfoForModule>({ module_id, backend });
			} else
				result = base::BAD;
		};
		for (const auto& module_id: modules_to_compile) handle_module(module_id);

		if (result.isBad()) return result;

		if (backend == BackendType::LLVM) {
			// Link all outputs into a single binary.
			auto output_file = global_state::getRootCollection()->fileArtifactAtOrNew(
				base::StrID(base::strConcat("package_", backendTypeToStr(backend), ".exe").c_str())
			);

			objects.push_back(emitBuiltinLLVMObjectFile());

			auto linking_result = linker::link(output_file, objects, linking_options);

			if (linking_result.isBad()) {
				CORE_USER_LOG("Linking failed!\n");
				return base::BAD;
			}
		}

		return result;
	}

	std::expected<RunOutput, std::string> runModuleOnDVM(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		CRef lir_data            = &ctx.query<CompileToLIRModuleData>(module_id)->valueOrPanic();
		auto dvm_code_collection = compileLIRModuleToDVM(lir_data, ctx, false);

		vm::PID pid{};

		return vm::api::spawn()
		    .and_then([&](vm::api::ProcessInfo process) {
				pid = process.pid;
				return std::expected<void, vm::api::ApiError>{};
			})
		    .and_then([&] { return vm::api::loadCode(pid, { dvm_code_collection.code }); })
		    .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
		    .and_then([&] { return vm::api::run(pid); })
		    .and_then([&] { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .transform([](vm::api::ExitValue exit_values) {
				CORE_ASSERT(
					exit_values.size() == 1,
					"Support for multiple return values in compiler not implemented"
				);
				return RunOutput{ .exit_code
				                  = base::safeIntConv<int>(exit_values.at(0)->readBytes<i64>()) };
			});
	}
}
