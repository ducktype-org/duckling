#include "absolute_symbols.hpp"
#include "jit_data.hpp"
#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <ranges>
#include <unordered_set>

LLVM_INCLUDE_BEGIN()
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
LLVM_INCLUDE_END()

    /**
	 * @brief Extracts function name from its mangled version. It should be string
	 * between last "::" (if present) and first "(".
	 */
	std::string extractFunctionName(const std::string& full) {
		size_t paren_pos = full.find('(');
		if (paren_pos == std::string::npos) paren_pos = full.length();

		size_t colons_pos = full.rfind("::", paren_pos);
		size_t start      = (colons_pos == std::string::npos) ? 0 : colons_pos + 2;

		return full.substr(start, paren_pos - start);
	}

	/**
	 * @brief "Exports" an LLVM global value so it is visible to other modules.
	 */
	void externalizeGlobalValue(llvm::GlobalValue& gv) {
		if (!gv.isDeclaration()) {
			gv.setLinkage(llvm::GlobalValue::ExternalLinkage);
			gv.setVisibility(llvm::GlobalValue::DefaultVisibility);
		}
	}

	/**
	 * @brief "Exports" all LLVM global values in a module (functions, global vars, metadata etc.)
	 * to make them visible to other modules.
	 * @note This is necessary for proper linking of the user function module with opfunction modules.
	 */
	void externalizeAllGlobalValues(llvm::Module& module) {
		for (auto& gv: module.globals()) externalizeGlobalValue(gv);

		for (auto& ga: module.aliases()) externalizeGlobalValue(ga);

		for (auto& ifunc: module.ifuncs()) externalizeGlobalValue(ifunc);

		for (auto& f: module.functions()) externalizeGlobalValue(f);
	}

    /**
	 * @brief For microinstruction name, returns corresponding MicroOpcode.
	 */
	vm::low::MicroOpcode getOpcode(const std::string& func_name) {
		for (auto [opcode, name]: std::views::enumerate(vm::low::OPCODE_NAMES))
			if (func_name == name) return static_cast<vm::low::MicroOpcode>(opcode);
		CORE_PANIC("Function name does not correspond to any MicroOpcode", func_name);
	}

    /**
	 * @brief Creates a new module that contains cloned definitions from `src` based on `filter` and
	 * adds it to `lljit`.
	 * @details ValueToValueMapTy indicates which values had already been cloned earlier - it is
	 * here just to satisfy LLVM's API.
	 */
	void cloneAndRegisterModule(
		llvm::Module&                 src,
		llvm::orc::LLJIT&             lljit,
		const auto&                   filter,
		llvm::orc::ThreadSafeContext& tsctx,
        llvm::ExitOnError&            exit_on_err
	) {
		llvm::ValueToValueMapTy     vmap;
		auto                        dest = llvm::CloneModule(src, vmap, filter);
		llvm::orc::ThreadSafeModule tsm(std::move(dest), tsctx);
		exit_on_err(lljit.addIRModule(std::move(tsm)));
	}

LlvmData init_llvm_jit() {
    auto [initial_context, g_module] = parseOpcodesBitcode();
    ExitOnError exit_on_err;

    auto lljit_instance = exit_on_err(LLJITBuilder().create());
	auto& jd = lljit_instance->getMainJITDylib();
	jd.addGenerator(cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
		lljit_instance->getDataLayout().getGlobalPrefix()
	)));

	registerAbsoluteJITSymbols(*lljit_instance);
    
    std::unordered_set<std::string> name_set;
	CORE_ASSERT(g_module->isMaterialized(), "Opfuns module not fully materialized!");
	externalizeAllGlobalValues(*g_module);

    std::unordered_map<vm::low::MicroOpcode, std::string> lfunc_name_map;

	for (auto& f: g_module->functions()) {
		if (!f.isDeclaration()) {
			auto func_name = f.getName().str();
			auto demangled = llvm::demangle(func_name);
			if (demangled.starts_with("vm::OpFuns::op_")
			    and !demangled.starts_with("vm::OpFuns::op_debug")) {
				name_set.insert(func_name);
				auto name              = extractFunctionName(demangled);
				name                   = name.substr(3);  // delete op_
				auto opcode            = getOpcode(name);
				lfunc_name_map[opcode] = func_name;
			}
		}
	}
	CORE_ASSERT(!lfunc_name_map.empty(), "Opfuns not found!");

	auto g_context = std::make_unique<ThreadSafeContext>(std::move(initial_context));

	// We need to clone module definitions to preserve them for future cloning.
	auto globals_filter = [&](const llvm::GlobalValue* gv) -> bool {
		return !name_set.contains(gv->getName().str());
	};
	cloneAndRegisterModule(*g_module, *lljit_instance, globals_filter, *g_context, exit_on_err);

	for (const auto& opfun_name: name_set) {
		auto opfun_filter = [&](const llvm::GlobalValue* gv) -> bool {
			return gv->getName().str() == opfun_name;
		};

		cloneAndRegisterModule(*g_module, *lljit_instance, opfun_filter, *g_context, exit_on_err);
	}
    
        auto flag_data_ty = llvm::StructType::create(*g_context->getContext(), "struct.vm::FlagData");
		flag_data_ty->setBody(
			{
				llvm::IntegerType::get(*g_context->getContext(), 1)  // bool flag
			},
			/*isPacked=*/false
		);

		auto frame_ty = llvm::StructType::create(*g_context->getContext(), "struct.vm::Frame");
		frame_ty->setBody(
			{ flag_data_ty },
			/*isPacked=*/false
		);
		CORE_ASSERT(frame_ty, "Frame type not found in module.");

		auto microinstruction_ty = llvm::StructType::create(*g_context->getContext(), "vm::MicroInstruction");
		CORE_ASSERT(microinstruction_ty, "MicroInstruction type not found in module.");

		auto vm_thread_ty = llvm::StructType::create(*g_context->getContext(), "vm::VMThread");
		CORE_ASSERT(vm_thread_ty, "VMThread type not found in module.");

		llvm::PointerType* mi_ptr_ptr_ty
			= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(microinstruction_ty));
		llvm::PointerType* byte_ptr_ptr_ty = llvm::PointerType::getUnqual(
			llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*g_context->getContext()))
		);
		llvm::PointerType* frame_ptr_ptr_ty
			= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(frame_ty));
		llvm::PointerType* vm_thread_ptr_ty = llvm::PointerType::getUnqual(vm_thread_ty);

		auto opfun_ty = llvm::FunctionType::get(
			llvm::Type::getVoidTy(*g_context->getContext()),
			{ mi_ptr_ptr_ty, byte_ptr_ptr_ty, frame_ptr_ptr_ty, vm_thread_ptr_ty },
			false
		);


    return LlvmData{
        .g_context = std::move(g_context),
        .g_module = std::move(g_module),
        .lljit_instance = std::move(lljit_instance),
        .exit_on_err = std::move(exit_on_err),
        .lfunc_name_map = std::move(lfunc_name_map),
        .frame_ty = std::move(frame_ty),  // will be filled in llvmInit
        .flag_data_ty = std::move(flag_data_ty),  // will be filled in llvmInit
        .microinstruction_ty = std::move(microinstruction_ty),  // will be filled in llvmInit
        .vm_thread_ty = std::move(vm_thread_ty),  // will be filled in llvmInit
        .opfun_ty = std::move(opfun_ty)  // will be filled in llvmInit
    };
}

LlvmData& llvmData() {
	static LlvmData llvm_data = init_llvm_jit();
	return llvm_data;
}
