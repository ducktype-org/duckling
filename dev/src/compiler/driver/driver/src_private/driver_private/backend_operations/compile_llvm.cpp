/**
 * @file compile_llvm.cpp
 * \parallel Must be thread-safe. Concurrent builds of the same module/package can collide on paths.
 */

#include "compile_llvm.hpp"

#include "builtins_registry.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <global_state/artifacts_location.hpp>
#include <helios/mangler/mangler.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/function_forward.hpp>
#include <time_stats/time_stats.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace compiler::driver {

	// TODO PR: consider moveing this to LLVM backend, it depends on LIR structure anyway!
	backend_llvm::Module compileLIRModuleToLLVM(
		query::Context& ctx, CRef<LIRUnitWithBackendName> lir_module
	) {
		time_stats::TrackCategoryTime _(time_stats::TimeCategories::BackendCompilation);

		backend_llvm::Module mod(lir_module->module_id);

		std::vector<CRef<lir::Function>> ctors;
		std::vector<CRef<lir::Function>> dtors;

		for (const auto& global: lir_module->lir_unit.lir_globals) {
			mod.addGlobalToModule(global.global);
			// Add global constructors and destructors if they exist
			variant_match(global.data_initialization) {
				variant_case(lir::LIRGlobalData::CTorDtorPair, ctor_dtor_pair) {
					if (ctor_dtor_pair.global_ctor.has_value()) {
						mod.addFunctionToModule(ctx, ctor_dtor_pair.global_ctor.value());
						ctors.push_back(ctor_dtor_pair.global_ctor.value());
					}
					if (ctor_dtor_pair.global_dtor.has_value()) {
						mod.addFunctionToModule(ctx, ctor_dtor_pair.global_dtor.value());
						dtors.push_back(ctor_dtor_pair.global_dtor.value());
					}
				}
				variant_case (ctv::CompileTimeValue, ctv_initial_value) {
					// This case is handled inside addGlobalToModule
				}
				variant_default {
					CORE_UNREACHABLE();
				}
			}
		}

		if (not ctors.empty()) {
			auto module_ctor = lir::createFunctionInvoker(
				ctx,
				ctors,
				helios::mangler::getSpecialMangledName<
					helios::mangler::ManglingSymbolKind::ModuleConstructor>(
					ctx, helios::mangler::special_symbol_keys::LIRModuleID{ lir_module->module_id }
				)
			);
			mod.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));
		}

		if (not dtors.empty()) {
			// Dtors should be called in reverse order
			std::vector<CRef<lir::Function>> reversed_dtors(dtors.rbegin(), dtors.rend());
			auto                             module_dtor = lir::createFunctionInvoker(
                ctx,
                reversed_dtors,
                helios::mangler::getSpecialMangledName<
												helios::mangler::ManglingSymbolKind::ModuleDestructor>(
                    ctx, helios::mangler::special_symbol_keys::LIRModuleID{ lir_module->module_id }
                )
            );
			mod.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
		}

		for (const auto& lir_function: lir_module->lir_unit.lir_functions) {
			mod.addFunctionToModule(ctx, lir_function);
		}			
		CORE_ASSERT(mod.verify().isOk(), "LLVM module verification failed");

		return mod;
	}

	artifacts::FileArtifact emitBuiltinLLVMObjectFile() {
		auto builtin_obj_file
			= global_state::getRootCollection()->fileArtifactAtOrNew(base::StrID("builtins_llvm.o"));
		auto mod = backend_llvm::Module::fromLLVMBC(getBuiltinsX8664LinuxGnuBCSpan());
		mod.compile(
			builtin_obj_file.file.getFilePath(), backend_llvm::CompilationOutputType::Object
		);
		return builtin_obj_file;
	}
}
