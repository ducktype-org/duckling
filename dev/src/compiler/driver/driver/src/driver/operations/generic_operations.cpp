/**
 * @file generic_operations.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "generic_operations.hpp"

#include <debug_info/debug_info_io.hpp>
#include <driver/debug_info/debug_info.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver/repl_utils/repl_dvm_helpers.hpp>
#include <driver/repl_utils/repl_split_helpers.hpp>
#include <driver/repl_utils/repl_statement_helpers.hpp>
#include <driver/repl_utils/script_dvm_helpers.hpp>
#include <driver/repl_utils/script_llvm_helpers.hpp>
#include <driver_private/backend_operations/compile_dvm.hpp>
#include <driver_private/backend_operations/compile_llvm.hpp>
#include <driver_private/operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>
#include <global_state/script_context.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <helios/repl_utils/script_helpers.hpp>
#include <linker/link.hpp>
#include <time_stats/time_stats.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/types/ok_bad.hpp>

#include <hashing/component_hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/standard_query/query_artifacts_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/loader/loader.hpp>

#include <filesystem>
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

	namespace {
		std::expected<vm::code::CodeCollection, std::string> compileScriptToDVMCollection() {
			auto& script_context = global_state::getScriptContext();
			auto  script_source  = script_context.script_file.getContent().view().stdString();
			auto  split_result   = repl::splitInputIntoStatements(script_source);
			if (!split_result.has_value())
				return std::unexpected(
					base::strConcat("Script parsing failed: ", split_result.error())
				);

			CORE_DEV_LOG(
				REPL, "compile_script: split into ", split_result->size(), " statement(s)\n"
			);

			std::expected<vm::code::CodeCollection, std::string> compiled_script;

			query::utils::withContextDo([&](query::Context& ctx) {
				backend_vm::ReplLoweringContext lowering_context(ctx);
				lowering_context.setContext(ctx);
				defer(lowering_context.invalidateContext());

				auto validated_program = vm::code::ValidProgram::withBuiltins();
				std::vector<repl::ScriptExecutableCall> executable_calls;
				base::Optional<frontend::ModuleID>      parent_module_id;

				auto try_insert_chunk = [&](const vm::code::CodeCollection& chunk) -> bool {
					try {
						validated_program = validated_program.tryInsertCode(chunk);
						return true;
					} catch (const std::exception& e) {
						compiled_script = std::unexpected(base::strConcat(
							"DVM bytecode validation failed while composing script chunks: ",
							e.what()
						));
						return false;
					}
				};

				u64 statement_counter = 0;

				for (const auto& statement_source: *split_result) {
					auto module_ref = repl::createEphemeralChainedStatementModule(
						statement_source, parent_module_id, statement_counter, "script_"
					);
					auto module_id   = module_ref->getModuleID();
					parent_module_id = module_id;

					CORE_DEV_LOG(
						REPL,
						"compile_script: statement #",
						statement_counter + 1,
						", module #",
						module_id.queryUnstablePerfectHash(),
						"\n"
					);

					auto statement_info_result = repl::classifySingleStatement(ctx, module_id);
					if (!statement_info_result.has_value()) {
						compiled_script = std::unexpected(statement_info_result.error());
						return;
					}

					auto module_name = repl::getStatementModuleName(module_id, "script_module_");

					if (std::holds_alternative<repl::DefinitionSingleStatementInfo>(
							statement_info_result.value()
						)) {
						CORE_DEV_LOG(REPL, "compile_script: classified as definition\n");
						const auto& hout_unit    = repl::getDefinitionHOUTUnit(ctx, module_id);
						auto        chunk_result = repl::compileHOUTUnitToDVMCode(
                            ctx, hout_unit, module_name, lowering_context
                        );
						if (!chunk_result.has_value()) {
							compiled_script = std::unexpected(chunk_result.error());
							return;
						}
						if (!try_insert_chunk(chunk_result.value())) return;
					} else {
						CORE_DEV_LOG(REPL, "compile_script: classified as executable statement\n");

						auto wrapper_result = repl::buildStatementWrapper(
							ctx, statement_info_result.value(), statement_counter
						);
						if (!wrapper_result.has_value()) {
							compiled_script = std::unexpected(wrapper_result.error());
							return;
						}

						auto hout_unit
							= repl::makeExecutableHOUTUnit(wrapper_result->wrapper_function);
						auto chunk_result = repl::compileHOUTUnitToDVMCode(
							ctx, hout_unit, module_name, lowering_context
						);
						if (!chunk_result.has_value()) {
							compiled_script = std::unexpected(chunk_result.error());
							return;
						}

						auto call_meta = repl::getDvmExecutableCallMetadata(
							chunk_result.value(), wrapper_result->wrapper_func_name
						);
						if (!call_meta.has_value()) {
							compiled_script = std::unexpected(base::strConcat(
								"Failed to extract wrapper metadata: ", call_meta.error()
							));
							return;
						}

						executable_calls.push_back(call_meta.value());
						if (!try_insert_chunk(chunk_result.value())) return;
					}

					++statement_counter;
				}

				vm::code::CodeCollection main_collection;
				// Generate the main entry point that calls all statement wrappers in order.
				// executable_calls contains the wrapper names + return types collected above.
				main_collection.functions.push_back(repl::makeDvmScriptMainFunction(executable_calls
				));
				if (!try_insert_chunk(main_collection)) return;

				compiled_script = validated_program.produceValidCodeCollection();
			});

			if (!compiled_script.has_value()) return std::unexpected(compiled_script.error());
			return compiled_script.value();
		}

		// @TODO: #2246 Unify backend-neutral script pipeline stages
		// (HOUT -> MIR -> LIR orchestration) with regular module compilation paths.
		// Backend selection should happen only after shared LIR is produced.
		std::expected<LIRModuleData, std::string> compileScriptToLLVMLIRModuleData(query::Context& ctx
		) {
			auto& script_context = global_state::getScriptContext();
			auto  script_source  = script_context.script_file.getContent().view().stdString();
			// Use the context-aware overload to avoid nesting withContextDo() while
			// the LLVM pipeline already owns a query context.
			auto split_result = repl::splitInputIntoStatements(ctx, script_source);
			if (!split_result.has_value())
				return std::unexpected(
					base::strConcat("Script parsing failed: ", split_result.error())
				);

			CORE_DEV_LOG(
				REPL, "compile_script_llvm: split into ", split_result->size(), " statement(s)\n"
			);

			LIRModuleData merged{
				.module_id = repl::getLLVMScriptModuleID(script_context.script_file),
				.functions = {},
				.globals   = {},
			};
			// Track wrapper symbols to build the synthetic main that runs them in order.
			// We preserve the original statement order to match script semantics.
			std::vector<helios::SymID>         wrapper_symbols;
			base::Optional<frontend::ModuleID> parent_module_id;
			u64                                statement_counter = 0;

			for (const auto& statement_source: *split_result) {
				auto module_ref = repl::createEphemeralChainedStatementModule(
					statement_source, parent_module_id, statement_counter, "script_"
				);
				auto module_id   = module_ref->getModuleID();
				parent_module_id = module_id;

				CORE_DEV_LOG(
					REPL,
					"compile_script: statement #",
					statement_counter + 1,
					", module #",
					module_id.queryUnstablePerfectHash(),
					"\n"
				);

				auto statement_info_result = repl::classifySingleStatement(ctx, module_id);
				if (!statement_info_result.has_value())
					return std::unexpected(statement_info_result.error());

				auto module_name    = repl::getStatementModuleName(module_id, "script_module_");
				auto module_name_id = base::StrID(module_name.c_str());

				if (std::holds_alternative<repl::DefinitionSingleStatementInfo>(
						statement_info_result.value()
					)) {
					CORE_DEV_LOG(REPL, "compile_script_llvm: classified as definition\n");
					const auto& hout_unit  = repl::getDefinitionHOUTUnit(ctx, module_id);
					auto        lir_result = ctx.query<CompileHOUTUnitToLIRModuleData>({
                        &hout_unit,
                        module_name_id,
                    });
					if (lir_result->hasFailed())
						return std::unexpected("Failed to compile definition statement to LIR");
					repl::appendLLVMLIRModuleData(merged, lir_result->valueOrPanic());
				} else {
					// Executable statements are wrapped into functions so we can sequence them
					// under a synthetic main while still lowering via the normal pipeline.
					// This is the same wrapper strategy used by the DVM script path.
					CORE_DEV_LOG(REPL, "compile_script_llvm: classified as executable statement\n");
					auto wrapper_result = repl::buildStatementWrapper(
						ctx, statement_info_result.value(), statement_counter
					);
					if (!wrapper_result.has_value()) return std::unexpected(wrapper_result.error());

					wrapper_symbols.push_back(
						wrapper_result->wrapper_function.declaration->original_symbol
					);

					auto hout_unit = repl::makeExecutableHOUTUnit(wrapper_result->wrapper_function);
					auto lir_result = ctx.query<CompileHOUTUnitToLIRModuleData>({
						&hout_unit,
						module_name_id,
					});
					if (lir_result->hasFailed())
						return std::unexpected("Failed to compile executable statement to LIR");
					repl::appendLLVMLIRModuleData(merged, lir_result->valueOrPanic());
				}

				++statement_counter;
			}

			if (!parent_module_id.has_value())
				return std::unexpected("Script has no statements to compile");

			// We use the last statement's module (parent_module_id) because it sits at the
			// end of the REPL-style chain and has a main-file root scope that represents
			// the full script context.
			auto main_scope = repl::queryScriptMainRootScope(ctx, parent_module_id.value());
			auto main_fun
				= repl::buildScriptMainWrapper(ctx, merged.module_id, main_scope, wrapper_symbols);
			helios::HOUTUnit main_unit;
			main_unit.functions.emplace_back(&main_fun);
			auto main_lir = ctx.query<CompileHOUTUnitToLIRModuleData>({
				&main_unit,
				merged.module_id,
			});
			if (main_lir->hasFailed())
				return std::unexpected("Failed to compile script main to LIR");
			repl::appendLLVMLIRModuleData(merged, main_lir->valueOrPanic());

			return merged;
		}

		// @TODO: #2246 After HOUT/MIR/LIR flow is unified, keep this function as
		// LLVM-only backend emission (object + link) while reusing shared pipeline output.
		base::OkBad compileScriptToLLVMExecutable(const linker::LinkingOptions& linking_options) {
			auto& script_context = global_state::getScriptContext();

			base::Optional<std::string> error_message;
			query::utils::withContextDo([&](query::Context& ctx) {
				auto script_lir = compileScriptToLLVMLIRModuleData(ctx);
				if (!script_lir.has_value()) {
					error_message = script_lir.error();
					return;
				}

				// Single object file for the whole script.
				auto object_name     = base::strConcat(script_lir->module_id.strView(), ".o");
				auto object_artifact = global_state::getRootCollection()->fileArtifactAtOrNew(
					base::StrID(object_name.c_str())
				);

				auto llvm_module = compileLIRModuleToLLVM(ctx, &script_lir.value());
				{
					time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);
					llvm_module.compile(
						object_artifact.file.getFilePath(),
						backend_llvm::CompilationOutputType::Object
					);
				}

				if (driver::llvm_dump_ir) {
					auto llvm_ir_path = base::StrID(
						base::strConcat(script_lir->module_id.strView(), ".ll").c_str()
					);
					llvm_module.dumpLLVMToFile(llvm_ir_path);
				}
				if (driver::llvm_dump_asm) {
					auto asm_path
						= base::StrID(base::strConcat(script_lir->module_id.strView(), ".s").c_str()
					    );
					llvm_module.compile(
						asm_path.strView(), backend_llvm::CompilationOutputType::Assembly
					);
				}

				// Link the script object with builtins to produce a runnable executable.
				auto output_name     = base::strConcat(script_context.script_file.stem(), ".exe");
				auto output_artifact = global_state::getRootCollection()->fileArtifactAtOrNew(
					base::StrID(output_name.c_str())
				);

				std::vector<artifacts::FileArtifact> objects;
				objects.push_back(object_artifact);
				objects.push_back(emitBuiltinLLVMObjectFile());

				auto linking_result = linker::link(output_artifact, objects, linking_options);
				if (linking_result.isBad()) {
					error_message = "Linking failed";
					return;
				}

				CORE_USER_LOG(
					"Script executable written to: ",
					output_artifact.file.getFilePath().string(),
					"\n"
				);
			});

			if (error_message.has_value()) {
				CORE_USER_LOG(error_message.value(), "\n");
				return base::BAD;
			}
			return base::OK;
		}

		base::OkBad compileScriptToDVMBytecode() {
			auto compiled_script = compileScriptToDVMCollection();
			if (!compiled_script.has_value()) {
				CORE_USER_LOG(compiled_script.error(), "\n");
				return base::BAD;
			}

			auto& script_context  = global_state::getScriptContext();
			auto  output_artifact = global_state::getRootCollection()->fileArtifactAtOrNew(
                base::StrID(base::strConcat(script_context.script_file.stem(), ".dbc").c_str())
            );
			std::ofstream output_file(
				output_artifact.file.getFilePath().getPath(), std::ios::binary
			);
			if (!output_file.is_open()) {
				CORE_USER_LOG(
					"Failed to open output file for script bytecode: ",
					output_artifact.file.getFilePath().string(),
					"\n"
				);
				return base::BAD;
			}

			vm::code::serializeCode(compiled_script.value(), output_file);
			output_file.close();

			CORE_USER_LOG(
				"Script bytecode written to: ", output_artifact.file.getFilePath().string(), "\n"
			);
			return base::OK;
		}

	}

	base::OkBad compileScript(
		BackendType backend_type, [[maybe_unused]] const linker::LinkingOptions& linking_options
	) {
		switch (backend_type) {
		case BackendType::LLVM:
			return compileScriptToLLVMExecutable(linking_options);
		case BackendType::DVM:
			return compileScriptToDVMBytecode();
		default:
			CORE_PANIC("bad backend type");
		}
	}

	std::expected<RunOutput, std::string> runScriptOnDVM() {
		auto compiled_script = compileScriptToDVMCollection();
		if (!compiled_script.has_value()) return std::unexpected(compiled_script.error());

		vm::PID pid{};

		return vm::api::spawn()
		    .and_then([&](vm::api::ProcessInfo process) {
				pid = process.pid;
				return std::expected<void, vm::api::ApiError>{};
			})
		    .and_then([&] { return vm::api::loadCode(pid, compiled_script.value()); })
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

		// Schedule compilation of every module up front so worker threads can run
		// them concurrently, then collect the results in a second pass.
		struct ScheduledModule {
			frontend::ModuleID     module_id;
			query::EntryTaskHandle handle;
		};

		std::vector<ScheduledModule> compile_handles;
		compile_handles.reserve(modules_to_compile.size());
		for (const auto& module_id: modules_to_compile) {
			compile_handles.push_back({ module_id,
			                            query::scheduleEntryPoint<CompileModule>(
											{ module_id, backend, build_debug_info }
										) });
		}

		std::vector<query::EntryTaskHandle> debug_info_handles;
		if (build_debug_info) debug_info_handles.reserve(modules_to_compile.size());

		for (auto& [module_id, handle]: compile_handles) {
			auto module_result = query::awaitEntryPoint<CompileModule>(handle);

			if (module_result->hasValue()) {
				objects.emplace_back(module_result->valueOrPanic().object_art);

				// We schedule debug info here, to only schedule it for correctly compiled modules.
				if (build_debug_info) {
					debug_info_handles.push_back(
						query::scheduleEntryPoint<DebugInfoForModule>({ module_id, backend })
					);
				}
			} else {
				result = base::BAD;
			}
		}

		std::vector<artifacts::FileArtifact> debug_info_artifacts;
		if (build_debug_info) debug_info_artifacts.reserve(debug_info_handles.size());
		for (auto handle: debug_info_handles) {
			auto di_result = query::awaitEntryPoint<DebugInfoForModule>(handle);
			if (build_debug_info && di_result.hasValue()) {
				debug_info_artifacts.emplace_back(di_result.valueOrPanic());
			}
		}

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

		if (backend == BackendType::DVM) {
			// Link all per-module .dbc files into a single merged CodeCollection (analogous to
			// linking .o object files for LLVM). This resolves cross-module function and global
			// references that were intentionally left unvalidated during per-module compilation.
			vm::loader::Loader dvm_linker;
			vm::code::CodeCollection merged_code;

			for (const auto& obj : objects) {
				auto parse_result = dvm_linker.parseCodeCollectionFromFiles({ obj.file });
				if (!parse_result.has_value()) {
					CORE_USER_LOG("DVM linking failed: could not parse module bytecode file\n");
					return base::BAD;
				}
				merged_code.mergeFrom(std::move(*parse_result));
			}

			auto output_file = global_state::getRootCollection()->fileArtifactAtOrNew(
				base::StrID(
					base::strConcat("package_", backendTypeToStr(backend), ".dbc").c_str()
				)
			);
			std::ofstream out(output_file.file.getFilePath().getPath(), std::ios::binary);
			if (!out.is_open())
				CORE_PANIC("Failed to open DVM package output file for writing");
			vm::code::serializeCode(merged_code, out);

			// Merge per-module debug info files into a single package debug info file.
			if (!debug_info_artifacts.empty()) {
				base::Optional<debug_info::DebugInfo> merged_debug_info;

				for (const auto& di_art : debug_info_artifacts) {
					std::ifstream in(di_art.file.getFilePath().getPath(), std::ios::binary);
					if (!in.is_open()) {
						CORE_USER_LOG("DVM: failed to open debug info artifact for merging\n");
						return base::BAD;
					}
					auto di_or_error = debug_info::loadFromStream(in);
					if (!di_or_error.has_value()) {
						CORE_USER_LOG(
							"DVM: failed to parse debug info file: ", di_or_error.error(), "\n"
						);
						return base::BAD;
					}
					if (!merged_debug_info.has_value()) {
						merged_debug_info.emplace(std::move(di_or_error.value()));
					} else {
						merged_debug_info->mergeFrom(std::move(di_or_error.value()));
					}
				}

				if (merged_debug_info.has_value()) {
					merged_debug_info->module_path = output_file.file.getFilePath().string();

					auto di_output = global_state::getRootCollection()->fileArtifactAtOrNew(
						base::StrID(
							base::strConcat("package_", backendTypeToStr(backend), ".di.json")
								.c_str()
						)
					);
					std::ofstream di_out(
						di_output.file.getFilePath().getPath(), std::ios::binary
					);
					if (!di_out.is_open())
						CORE_PANIC("Failed to open DVM package debug info output file for writing");
					debug_info::saveToStream(*merged_debug_info, di_out);
				}
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
		    .transform([](Ref<vm::VmValue> exit_value) {
				return RunOutput{ .exit_code
				                  = base::safeIntConv<int>(exit_value->readBytes<i64>()) };
			});
	}
}
