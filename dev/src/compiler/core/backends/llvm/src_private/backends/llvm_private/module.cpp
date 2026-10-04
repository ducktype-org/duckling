#include "compile_llvm.hpp"
#include "llvm_includes/cloning.hpp"
#include "llvm_includes/filesystem.hpp"
#include "llvm_includes/ir_verifier.hpp"
#include "llvm_lowering.hpp"
#include "module_impl.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <helios/mangler/mangler.hpp>  // @TODO: #2796 remove this include if possible
#include <lir/lir_lowering/lir_lowering.hpp>

#include <base/except/exceptions.hpp>

#include <logger/logger.hpp>

#include <iostream>

namespace compiler::backend_llvm {
	Module::Module(const base::StrID module_id): impl(initModuleImpl(module_id)) {}

	Module Module::fromIRCode(std::string_view llvm_ir_code) {
		return { parseIRCodeToModuleImpl(llvm_ir_code) };
	}

	Module Module::fromLLVMBC(const std::span<unsigned char> llvm_bc_data) {
		return { parseLLVMBCToModuleImpl(llvm_bc_data) };
	}

	Module Module::fromLIRUnit(
		query::Context& ctx, const lir::LIRUnit& lir_unit, base::StrID module_id
	) {
		backend_llvm::Module mod(module_id);

		std::vector<CRef<lir::Function>> ctors;
		std::vector<CRef<lir::Function>> dtors;

		for (const auto& global: lir_unit.lir_globals) {
			mod.addGlobalDeclarationToModule(global);

			variant_match(global.data_initialization) {
				variant_case(lir::LIRGlobalData::CTorDtorPair, ctor_dtor_pair) {
					CORE_ASSERT(
						global.global.type == lir::LIRGlobalType::Variable,
						"Only variable globals can have ctor/dtor pair as initial value (constants "
						"should always have CTV initial value)"
					);

					// Add global constructors and destructors if they exist
					if (ctor_dtor_pair.global_ctor.has_value()) {
						mod.addFunctionToModule(ctx, ctor_dtor_pair.global_ctor.value());
						ctors.push_back(ctor_dtor_pair.global_ctor.value());
					}
					if (ctor_dtor_pair.global_dtor.has_value()) {
						mod.addFunctionToModule(ctx, ctor_dtor_pair.global_dtor.value());
						dtors.push_back(ctor_dtor_pair.global_dtor.value());
					}
				}
				variant_case(ctv::CompileTimeValue, ctv_initial_value) {
					mod.setGlobalConstantInitializer(global.global.mangled_name, ctv_initial_value);
				}
				variant_default { CORE_UNREACHABLE(); }
			}
		}

		if (not ctors.empty()) {
			auto module_ctor = lir::createFunctionInvoker(
				ctx,
				ctors,
				helios::mangler::getSpecialMangledName<
					helios::mangler::ManglingSymbolKind::ModuleConstructor>(
					ctx, helios::mangler::special_symbol_keys::LIRModuleID{ module_id }
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
                    ctx, helios::mangler::special_symbol_keys::LIRModuleID{ module_id }
                )
            );
			mod.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
		}

		for (const auto& lir_function: lir_unit.lir_functions)
			mod.addFunctionToModule(ctx, lir_function);

		CORE_ASSERT(
			mod.verify().isOk(),
			"LLVM module verification failed (enable Backend dev logs to see details)"
		);

		return mod;
	}

	void Module::addFunctionToModule(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleImpl(ctx, impl.refMut(), lir_function);
	}

	void Module::addGlobalDeclarationToModule(const lir::LIRGlobalData& lir_global) {
		addGlobalDeclarationToModuleImpl(impl.refMut(), lir_global);
	}

	void Module::setGlobalConstantInitializer(
		base::StrID global_name, const ctv::CompileTimeValue& constant_value
	) {
		setGlobalConstantInitializerImpl(impl.refMut(), global_name, constant_value);
	}

	void Module::addFunctionToModuleCtors(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleCtorsImpl(ctx, impl.refMut(), lir_function);
	}

	void Module::addFunctionToModuleDtors(query::Context& ctx, CRef<lir::Function> lir_function) {
		addFunctionToModuleDtorsImpl(ctx, impl.refMut(), lir_function);
	}

	base::OkBad Module::verify() const {
		std::string              llvm_verification;
		llvm::raw_string_ostream llvm_verification_stream(llvm_verification);

		bool error_found = llvm::verifyModule(*impl->module, &llvm_verification_stream);

		if (error_found) {
			CORE_DEV_LOG(Backend, dumpLLVMToString());
			CORE_DEV_LOG(Backend, "LLVM Verification Failed!: ", "\n", llvm_verification, "\n");
		}

		return error_found ? base::BAD : base::OK;
	}

	Module Module::clone() const {
		std::unique_ptr<llvm::Module> new_module = llvm::CloneModule(*impl->module);

		Box<llvm::Module> llvm_module = Box<llvm::Module>::fromPointer(new_module.release());
		auto              module_impl = makeBox<ModuleImpl>(std::move(llvm_module));
		return { std::move(module_impl) };
	}

	void Module::debugPrint() const { return impl->module->print(llvm::errs(), nullptr); }

	void Module::dumpLLVMToFile(base::StrID output_file) const {
		std::error_code error_code;
		llvm::raw_fd_ostream ir_output_stream(output_file.str(), error_code, llvm::sys::fs::OF_None);
		if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

		impl->module->print(ir_output_stream, nullptr);
	}

	std::string Module::dumpLLVMToString() const {
		std::string              buffer;
		llvm::raw_string_ostream stream(buffer);

		impl->module->print(stream, nullptr);
		stream.flush();
		return buffer;
	}

	void Module::compile(
		const std::filesystem::path& output_file, const CompilationOutputType output_type
	) {
		compileModuleToObject(impl.refMut(), output_file, output_type);
		CORE_ASSERT(std::filesystem::exists(output_file), "LLVM compilation to file failed!");
	}

	u64 Module::getFunctionCount(bool including_prototypes) const {
		const auto& func_list = impl->module->getFunctionList();
		u64         count     = 0;
		for (auto& func: func_list) {
			if (func.isDeclaration() and (not including_prototypes)) continue;
			count++;
		}
		return count;
	}

	Module::~Module() = default;
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(compiler::backend_llvm::ModuleImpl)
