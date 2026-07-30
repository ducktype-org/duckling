/**
 * @file generic_operations.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "generic_operations.hpp"

#include <debug_info/debug_info_io.hpp>
#include <driver/debug_info/debug_info.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver/options.hpp>
#include <driver/repl_utils/repl_dvm_helpers.hpp>
#include <driver/repl_utils/repl_split_helpers.hpp>
#include <driver/repl_utils/repl_statement_helpers.hpp>
#include <driver/repl_utils/script_helpers.hpp>
#include <driver_private/backend_operations/compile_dvm.hpp>
#include <driver_private/backend_operations/compile_llvm.hpp>
#include <driver_private/dbc_linking.hpp>
#include <driver_private/debug_artifacts.hpp>
#include <driver_private/operations.hpp>
#include <driver_private/standard_library/standard_library.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>
#include <global_state/script_context.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <helios/repl_utils/script_helpers.hpp>
#include <linker/link.hpp>
#include <time_stats/time_stats.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/ok_bad.hpp>

#include <artifacts/artifacts.hpp>
#include <hashing/component_hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/standard_query/query_artifacts_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/loader/loader.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

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

		// Every real backend type; the set allPossibleArtifactNames() enumerates when building the
		// deletable artifact names. Keep in sync with typeExtension() when adding a backend.
		static constexpr std::array ALL_BACKEND_TYPES{ BackendType::LLVM, BackendType::DVM };

		/**
		 * @brief Every on-disk artifact name a CompileModule can produce for a given stable hash.
		 *
		 * Single source of truth for artifact naming: getModuleOutputName() may only ever produce
		 * names contained here (enforced by assert), and deleteFromDisk() removes exactly this set.
		 * Adding a backend or artifact kind here keeps write, load and delete in sync automatically.
		 */
		static std::vector<std::string> allPossibleArtifactNames(query::QueryStableHash hash) {
			const auto               stem = hash.toStringHex();
			std::vector<std::string> names;
			names.reserve(ALL_BACKEND_TYPES.size() + 1);
			for (auto backend: ALL_BACKEND_TYPES) names.push_back(stem + typeExtension(backend));
			names.push_back(stem + std::string(DEBUG_INFO_STABLE_EXTENSION));
			return names;
		}

		struct ModuleOutputNames {
			std::string                 object_file;
			base::Optional<std::string> debug_info_file;
		};

		static ModuleOutputNames getModuleOutputName(const QKey& key) {
			ModuleOutputNames names;
			const auto        hash = key.queryStablePerfectHash();
			const auto        stem = hash.toStringHex();
			names.object_file      = stem + typeExtension(key.backend_type);
			if (key.build_debug_info && key.backend_type == BackendType::DVM)
				names.debug_info_file = stem + std::string(DEBUG_INFO_STABLE_EXTENSION);

			// Enforce that every name we write is one deleteFromDisk() knows how to remove: a written
			// artifact whose name is not in the single source of truth would silently leak on disk.
			IF_BUILD_TYPE_DEV({
				const auto possible = allPossibleArtifactNames(hash);
				CORE_ASSERT(
					std::ranges::find(possible, names.object_file) != possible.end(),
					"CompileModule object artifact name is not in the deletable set"
				);
				if (names.debug_info_file.has_value())
					CORE_ASSERT(
						std::ranges::find(possible, names.debug_info_file.value()) != possible.end(),
						"CompileModule debug-info artifact name is not in the deletable set"
					);
			});

			return names;
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			moduleLog(key, "Recompiling");

			auto lir_data_result = compileModuleToLIRModuleData(ctx, key.module_id);
			if (lir_data_result.hasFailed()) {
				moduleLog(key, "Compilation failed");
				return query::Failed();
			}
			auto lir_data = std::move(lir_data_result).valueOrPanic();

			auto output_names = getModuleOutputName(key);
			auto code_output  = getQueryArtifactsCollection()->fileArtifactAtOrNew(
                base::StrID(output_names.object_file)
            );

			base::Optional<debug_info::DebugInfo>   debug_info_output;
			base::Optional<artifacts::FileArtifact> debug_info_artifact;

			switch (key.backend_type) {
			case BackendType::LLVM: {
				auto llvm_module = compileLIRModuleToLLVM(ctx, &lir_data);
				{
					// compileLIRModuleToLLVM time is added on its own,
					// but tracking time of the actual compilation to object file is done here
					time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

					llvm_module.compile(
						code_output.file.getFilePath(), backend_llvm::CompilationOutputType::Object
					);
				}

				if (driver::dump_ir_options.dump_llvm) {
					auto llvm_ir_artifact = getDebugArtifactCollection()->fileArtifactAtOrNew(
						base::StrID(lir_data.module_id.str() + ".ll")
					);
					llvm_module.dumpLLVMToFile(
						base::StrID(llvm_ir_artifact.file.getFilePath().string())
					);
				}
				if (driver::dump_ir_options.dump_asm) {
					auto asm_artifact = getDebugArtifactCollection()->fileArtifactAtOrNew(
						base::StrID(lir_data.module_id.str() + ".s")
					);
					llvm_module.compile(
						asm_artifact.file.getFilePath().getPath(),
						backend_llvm::CompilationOutputType::Assembly
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

				auto dvm_module_data = compileLIRModuleToDVM(&lir_data, ctx, key.build_debug_info);

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
		static auto loadFromDisk(const QKey& key) -> base::Optional<PResult> {
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

		/**
		 * Deletes all on-disk artifacts of a module identified by its stable key hash.
		 * The full key is not available here (invalidation and orphan cleanup only know the hash),
		 * so every possible artifact name is attempted. deleteFileArtifact() is a no-op for files
		 * that do not exist, so speculative deletes are safe.
		 */
		static auto deleteFromDisk(query::QueryStableHash hash) -> bool {
			auto collection = getQueryArtifactsCollection();

			CORE_DEV_LOG(
				Artifacts, "Deleting module artifacts from disk for hash: ", hash.toStringHex(), "\n"
			);

			bool deleted = false;
			for (const auto& name: allPossibleArtifactNames(hash))
				deleted |= collection->deleteFileArtifact(base::StrID(name.c_str()));
			return deleted;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileModule);

	namespace {
		std::expected<LIRUnitWithBackendName, std::string> compileScriptToLIRModuleData(
			query::Context& ctx
		) {
			auto& script_context = global_state::getScriptContext();
			auto  script_source  = script_context.script_file.getContent().view().stdString();
			auto  split_result   = repl::splitInputIntoStatements(ctx, script_source);
			if (!split_result.has_value())
				return std::unexpected(
					base::strConcat("Script parsing failed: ", split_result.error())
				);

			CORE_DEV_LOG(
				REPL, "compile_script: split into ", split_result->size(), " statement(s)\n"
			);

			LIRUnitWithBackendName merged{
				.module_id = repl::getScriptModuleID(script_context.script_file),
				.lir_unit  = lir::LIRUnit{},
			};

			// Track wrapper symbols to build the synthetic main that runs them in order.
			// We preserve the original statement order to match script semantics.
			std::vector<helios::SymID>         wrapper_symbols;
			base::Optional<frontend::ModuleID> parent_module_id;
			u64                                statement_counter = 0;

			if (split_result->empty()) {
				// Empty input should still produce a valid synthetic script main wrapper with
				// no statement calls.
				auto empty_script_module
					= repl::createEphemeralChainedStatementModule("", {}, 0, "script_");
				parent_module_id = empty_script_module->getModuleID();
			}

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
					CORE_DEV_LOG(REPL, "compile_script: classified as definition\n");
					const auto& hout_unit = repl::getDefinitionHOUTUnit(ctx, module_id);
					auto        lir_result
						= compileHOUTUnitToLIRModuleData(ctx, hout_unit, module_name_id);
					if (lir_result.hasFailed())
						return std::unexpected("Failed to compile definition statement to LIR");
					repl::appendScriptLIRModuleData(merged, lir_result.valueOrPanic());
				} else {
					// Executable statements are wrapped into functions so we can sequence them
					// under a synthetic main while still lowering via the normal pipeline.
					CORE_DEV_LOG(REPL, "compile_script: classified as executable statement\n");
					auto wrapper_result = repl::buildStatementWrapper(
						ctx, statement_info_result.value(), statement_counter
					);
					if (!wrapper_result.has_value()) return std::unexpected(wrapper_result.error());

					wrapper_symbols.push_back(
						wrapper_result->wrapper_function.declaration->original_symbol
					);

					auto hout_unit = repl::makeExecutableHOUTUnit(wrapper_result->wrapper_function);
					auto lir_result
						= compileHOUTUnitToLIRModuleData(ctx, hout_unit, module_name_id);
					if (lir_result.hasFailed())
						return std::unexpected("Failed to compile executable statement to LIR");
					repl::appendScriptLIRModuleData(merged, lir_result.valueOrPanic());
				}

				++statement_counter;
			}

			if (!parent_module_id.has_value())
				return std::unexpected("Script has no parent module for synthetic main");

			// We use the last (or empty one when script is empty) statement's module
			// (parent_module_id) because it sits at the end of the REPL-style chain and has a
			// main-file root scope that represents the full script context.
			auto main_scope = repl::queryScriptMainRootScope(ctx, parent_module_id.value());
			auto main_fun
				= repl::buildScriptMainWrapper(ctx, merged.module_id, main_scope, wrapper_symbols);
			helios::HOUTUnit main_unit;
			main_unit.functions.emplace_back(&main_fun);
			auto main_lir = compileHOUTUnitToLIRModuleData(ctx, main_unit, merged.module_id);
			if (main_lir.hasFailed())
				return std::unexpected("Failed to compile script main to LIR");
			repl::appendScriptLIRModuleData(merged, main_lir.valueOrPanic());

			// @TODO: #2694 #2424 come back to this, and maybe remove or adapt this call
			// accordingly. Currently we need it, to deduplicate toString methods that are emmitted
			// in each hout unit, and then duplicated as a result of merging multiple LIR units into
			// one. We are not sure, whether this is the best way to handle this, but it is a simple
			// solution for now.
			merged.lir_unit.deduplicateSymbols();

			return merged;
		}

		base::OkBad compileScriptToLLVMExecutable(const linker::LinkingOptions& linking_options) {
			auto& script_context = global_state::getScriptContext();

			base::Optional<std::string> error_message;
			query::utils::withContextDo([&](query::Context& ctx) {
				auto script_lir = compileScriptToLIRModuleData(ctx);
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

				if (driver::dump_ir_options.dump_llvm) {
					auto llvm_ir_artifact = getDebugArtifactCollection()->fileArtifactAtOrNew(
						base::StrID(base::strConcat(script_lir->module_id.strView(), ".ll"))
					);
					llvm_module.dumpLLVMToFile(
						base::StrID(llvm_ir_artifact.file.getFilePath().string())
					);
				}
				if (driver::dump_ir_options.dump_asm) {
					auto asm_artifact = getDebugArtifactCollection()->fileArtifactAtOrNew(
						base::StrID(base::strConcat(script_lir->module_id.strView(), ".s"))
					);
					llvm_module.compile(
						asm_artifact.file.getFilePath().getPath(),
						backend_llvm::CompilationOutputType::Assembly
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

				auto linking_result
					= linker::linkExecutable(output_artifact, objects, linking_options);
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

		base::OkBad compileScriptToDVMBytecode(bool link_std_lib) {
			base::Optional<std::string> error_message;
			query::utils::withContextDo([&](query::Context& ctx) {
				auto script_lir = compileScriptToLIRModuleData(ctx);
				if (!script_lir.has_value()) {
					error_message = script_lir.error();
					return;
				}

				auto dvm_module = compileLIRModuleToDVM(&script_lir.value(), ctx, false);

				auto script_obj_artifact = global_state::getRootCollection()->fileArtifactAtOrNew(
					base::StrID(base::strConcat(script_lir->module_id, ".o.dbc"))
				);
				std::ofstream module_output_file(
					script_obj_artifact.file.getFilePath().getPath(), std::ios::binary
				);
				if (!module_output_file.is_open()) {
					error_message = base::strConcat(
						"Failed to open output file for script bytecode: ",
						script_obj_artifact.file.getFilePath().string()
					);
					return;
				}

				vm::code::serializeCode(dvm_module.code, module_output_file);
				module_output_file.close();

				auto& script_context = global_state::getScriptContext();
				auto  output_file    = global_state::getRootCollection()->fileArtifactAtOrNew(
                    base::StrID(base::strConcat(script_context.script_file.stem(), ".dbc"))
                );

				std::vector<artifacts::FileArtifact> dvm_objs = { std::move(script_obj_artifact) };
				if (link_std_lib)
					for (auto&& dvm_std_obj: getStdLibDVMArtifacts())
						dvm_objs.emplace_back(std::move(dvm_std_obj));


				if (linkDVMPackage(dvm_objs, {}, output_file).isBad()) {
					error_message = "Linking of the DVM objects failed.";
					return;
				}

				CORE_USER_LOG(
					"Script bytecode written to: ", output_file.file.getFilePath().strView(), "\n"
				);
			});

			if (error_message.has_value()) {
				CORE_USER_LOG(error_message.value(), "\n");
				return base::BAD;
			}

			return base::OK;
		}
	}

	base::OkBad compileScript(
		BackendType                          backend_type,
		const options_types::StdLibOptions&  std_lib_opts,
		const options_types::LinkingOptions& linking_opts
	) {
		switch (backend_type) {
		case BackendType::LLVM:
			return compileScriptToLLVMExecutable(
				constructNativeLinkerOptions(linking_opts, std_lib_opts)
			);
		case BackendType::DVM:
			return compileScriptToDVMBytecode(std_lib_opts.stdActive());
		default:
			CORE_PANIC("bad backend type");
		}
	}

	std::expected<RunOutput, std::string> runScriptOnDVM(bool load_stdlib) {
		base::Optional<std::string> error_message;
		base::Optional<RunOutput>   output;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto script_lir = compileScriptToLIRModuleData(ctx);
			if (!script_lir.has_value()) {
				error_message = script_lir.error();
				return;
			}

			auto dvm_module = compileLIRModuleToDVM(&script_lir.value(), ctx, false);

			if (load_stdlib) {
				using std::ranges::to;
				using std::ranges::views::transform;
				auto parse_result = vm::loader::Loader{}.parseCodeCollectionFromFiles(
					getStdLibDVMArtifacts() | transform(&artifacts::FileArtifact::file)
					| to<std::vector>()
				);
				if (!parse_result.has_value()) {
					error_message
						= base::strConcat("Failed to load std bytecode: ", parse_result.error());
					return;
				}
				dvm_module.code.mergeFrom(std::move(parse_result).value());
				// @TODO: #2895 deal with this once weak/strong symbols are added
				deduplicateCodeCollection(dvm_module.code);
			}

			vm::PID pid{};
			auto    run_result
				= vm::api::spawn()
			          .and_then([&](vm::api::ProcessInfo process) {
						  pid = process.pid;
						  return std::expected<void, vm::api::ApiError>{};
					  })
			          .and_then([&] { return vm::api::loadCode(pid, dvm_module.code); })
			          .and_then([&] { return vm::api::attach(pid, std::cin, std::cout); })
			          .and_then([&] { return vm::api::run(pid); })
			          .and_then([&] { return vm::api::join(pid); })
			          .and_then([&] { return vm::api::getExitValue(pid); })
			          .transform_error(vm::api::errorToString)
			          .transform([](vm::api::ExitValue exit_values) {
						  CORE_ASSERT(
							  v_matches(exit_values, std::vector<Ref<vm::IVMValue>>),
							  "Expected exit value to be vector"
						  );
						  const auto& exit_values_vec
							  = std::get<std::vector<Ref<vm::IVMValue>>>(exit_values);
						  CORE_ASSERT(exit_values_vec.size() == 1, "Expected single exit value");
						  return RunOutput{ .exit_code = base::safeIntConv<int>(
												exit_values_vec.at(0)->readBytes<i64>()
											) };
					  });

			if (run_result.has_value())
				output = run_result.value();
			else
				error_message = run_result.error();
		});

		if (error_message.has_value()) return std::unexpected(error_message.value());
		return output.value();
	}

	namespace {
		struct ModuleToCompile {
			frontend::ModuleID package_root_module;
			frontend::ModuleID module_id;
			BackendType        backend;
			bool               build_debug_info;

			bool operator==(const ModuleToCompile&) const = default;
		};

		/**
		 * @brief Strict-weak ordering over all fields of ModuleToCompile for deterministic
		 * sort/dedup. Uses stable module hash so the order is reproducible across runs.
		 */
		bool lessModuleToCompile(const ModuleToCompile& a, const ModuleToCompile& b) {
			const auto& a_mod_hash = frontend::ModuleTree::getModuleHash(a.module_id);
			const auto& b_mod_hash = frontend::ModuleTree::getModuleHash(b.module_id);
			if (a_mod_hash != b_mod_hash) return a_mod_hash < b_mod_hash;
			if (a.backend != b.backend) return a.backend < b.backend;
			if (a.build_debug_info != b.build_debug_info)
				return a.build_debug_info < b.build_debug_info;
			const auto& a_root_hash = frontend::ModuleTree::getModuleHash(a.package_root_module);
			const auto& b_root_hash = frontend::ModuleTree::getModuleHash(b.package_root_module);
			return a_root_hash < b_root_hash;
		}
	}  // namespace

	base::OkBad compilePackages(const std::vector<PackageCompilationTask>& tasks) {
		base::OkBad result = base::OK;

		std::vector<ModuleToCompile> modules_to_compile;

		// Per-backend artifact maps: a single package may have both LLVM tasks
		// (lib/native) and a DVM task, in which case both backends compile the
		// same modules. Mixing .o (LLVM) and .dbc (DVM) artifacts in one vector
		// would feed the DVM linker .o files (and vice versa), so the maps are
		// keyed separately by backend.
		base::HashMap<frontend::ModuleID, std::vector<artifacts::FileArtifact>>
			llvm_objects_by_root_module;
		base::HashMap<frontend::ModuleID, std::vector<artifacts::FileArtifact>>
			dvm_objects_by_root_module;

		std::function<void(frontend::ModuleID, BackendType, frontend::ModuleID)> collect_modules
			= [&](frontend::ModuleID module_id,
		          BackendType        backend,
		          frontend::ModuleID package_root_module) -> void {
			// @TODO: #2354 This is temporary.
			const bool build_debug_info = backend == BackendType::DVM;

			modules_to_compile.push_back(
				{ package_root_module, module_id, backend, build_debug_info }
			);
			auto sub_modules = query::entryPoint<frontend::QuerySubmodules>(module_id);
			for (const auto& [id, sub_module]: *sub_modules)
				collect_modules(sub_module, backend, package_root_module);
		};

		for (const auto& task: tasks) {
			variant_match(task.build_target) {
				variant_case_novalue(BuildTargetDVMLibrary, BuildTargetDVMExecutable) {
					collect_modules(task.root_module, BackendType::DVM, task.root_module);
				}
				variant_default {
					collect_modules(task.root_module, BackendType::LLVM, task.root_module);
				}
			}
		}

		// Sort + dedup: a single (module_id, backend, build_debug_info, root_module) should be
		// compiled at most once even if multiple tasks reference it. The sort key is the
		// module's content hash, which is effectively random across modules — that gives us
		// deterministic output *and* spreads sibling modules across the worker queue (better
		// load balancing than feeding workers a depth-first traversal), so no separate shuffle
		// is needed.
		std::ranges::sort(modules_to_compile, lessModuleToCompile);
		modules_to_compile.erase(
			std::ranges::unique(modules_to_compile).begin(), modules_to_compile.end()
		);

		ImplementationOf_CompileModule::total_module_count.store(modules_to_compile.size());

		// Schedule compilation of every module up front so worker threads can run
		// them concurrently, then collect the results in a second pass.
		struct ScheduledModule final {
			ModuleToCompile        module;
			query::EntryTaskHandle handle;
		};

		std::vector<ScheduledModule> compile_handles;
		compile_handles.reserve(modules_to_compile.size());
		for (const auto& module: modules_to_compile) {
			compile_handles.push_back({
				module,
				query::scheduleEntryPoint<CompileModule>(
					{ module.module_id, module.backend, module.build_debug_info }
				),
			});
		}

		std::vector<ScheduledModule> debug_info_handles;

		for (auto& [module, handle]: compile_handles) {
			auto module_result = query::awaitEntryPoint<CompileModule>(handle);
			if (module_result->hasValue()) {
				auto& objects_by_root_module = module.backend == BackendType::DVM
				                                 ? dvm_objects_by_root_module
				                                 : llvm_objects_by_root_module;
				objects_by_root_module
					.put(module.package_root_module, std::vector<artifacts::FileArtifact>())
					.first->second.emplace_back(module_result->valueOrPanic().object_art);

				// We schedule debug info here, to only schedule it for correctly compiled modules.
				if (module.build_debug_info) {
					debug_info_handles.push_back({ module,
					                               query::scheduleEntryPoint<DebugInfoForModule>(
													   { module.module_id, module.backend }
												   ) });
				}
			} else {
				result = base::BAD;
			}
		}

		base::HashMap<frontend::ModuleID, std::vector<artifacts::FileArtifact>>
			debug_info_artifacts_by_root_module;

		for (auto [module, handle]: debug_info_handles) {
			auto di_result = query::awaitEntryPoint<DebugInfoForModule>(handle);
			if (di_result.hasValue())
				debug_info_artifacts_by_root_module
					.put(module.package_root_module, std::vector<artifacts::FileArtifact>())
					.first->second.emplace_back(di_result.valueOrPanic());
			else {
				CORE_USER_LOG("Debug info generation failed for a module.");
				result = base::BAD;
			}
		}

		if (result.isBad()) return result;

		for (const auto& task: tasks) {
			variant_match(task.build_target) {
				variant_case(BuildTargetLLVMExecutable, target_exe) {
					auto output_file = global_state::getRootCollection()->fileArtifactAtOrNew(
						target_exe.output_file_name
					);

					llvm_objects_by_root_module.atMaybe(task.root_module)
						.value()
						->push_back(emitBuiltinLLVMObjectFile());

					auto linking_result = linker::linkExecutable(
						output_file,
						llvm_objects_by_root_module.at(task.root_module),
						target_exe.linking_options
					);

					if (linking_result.isBad()) {
						CORE_USER_LOG(
							"Linking for package "
							+ compiler::frontend::getModuleRef(task.root_module)
								  ->getPackage()
								  .illegalAccess()
								  .getID()
								  .str()
							+ " failed!\n"
						);
						result = base::BAD;
					}
				}
				variant_case(BuildTargetLLVMStaticLibrary, target_lib) {
					auto root_collection = global_state::getRootCollection();
					if_opt_some(target_lib.custom_art_collection, custom_art_collection) {
						root_collection = custom_art_collection;
					}
					// Note, if you change this convention, please also change the one in the
					// `getStdLibArtifacts`
					auto output_file
						= root_collection->fileArtifactAtOrNew(target_lib.output_file_name);

					auto archive_result = archiver::createArchive(
						output_file,
						*llvm_objects_by_root_module.atMaybe(task.root_module).value(),
						target_lib.archiving_options
					);

					if (archive_result.isBad()) {
						CORE_USER_LOG(
							"Archiving for package "
							+ compiler::frontend::getModuleRef(task.root_module)
								  ->getPackage()
								  .illegalAccess()
								  .getID()
								  .str()
							+ " failed!\n"
						);
						result = base::BAD;
					}
				}
				variant_case_novalue(BuildTargetLLVM) {
					// Do nothing for plain object files
				}
				variant_case(BuildTargetDVMLibrary, target_dvm) {
					auto root_collection = global_state::getRootCollection();
					if_opt_some(target_dvm.custom_art_collection, custom_art_collection) {
						root_collection = custom_art_collection;
					}
					auto output_file
						= root_collection->fileArtifactAtOrNew(target_dvm.output_file_name);

					auto debug_info_opt
						= debug_info_artifacts_by_root_module.atMaybe(task.root_module);

					if (linkDVMPackage(
							*dvm_objects_by_root_module.atMaybe(task.root_module).value(),
							debug_info_opt.has_value() ? *debug_info_opt.value()
													   : std::vector<artifacts::FileArtifact>(),
							output_file
						)
					        .isBad())
						result = base::BAD;
				}
				variant_case(BuildTargetDVMExecutable, target_dvm) {
					// If the `target_dvm.link_std_packages` is on, we link the std packages as well.
					std::vector<artifacts::FileArtifact> dbc_arts
						= *dvm_objects_by_root_module.atMaybe(task.root_module).value();
					if (target_dvm.link_std_packages)
						for (auto& art: getStdLibDVMArtifacts()) dbc_arts.push_back(std::move(art));

					std::vector<artifacts::FileArtifact> debug_info_arts;

					if_opt_some(
						debug_info_artifacts_by_root_module.atMaybe(task.root_module), debug_arts
					) {
						for (const auto& art: *debug_arts) debug_info_arts.push_back(art);
						if (target_dvm.link_std_packages)
							for (auto& art: getStdLibDVMDebugInfoArtifacts())
								debug_info_arts.push_back(std::move(art));
					}

					auto output_file = global_state::getRootCollection()->fileArtifactAtOrNew(
						target_dvm.output_file_name
					);
					// We may mix artifacts from different collections here (std
					// artifacts can come from a separate collection).
					if (linkDVMPackage(dbc_arts, debug_info_arts, output_file).isBad())
						result = base::BAD;
				}
			}
		}

		return result;
	}
}
