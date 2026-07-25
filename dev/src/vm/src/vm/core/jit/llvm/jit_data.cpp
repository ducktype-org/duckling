#include "jit_data.hpp"

#include "absolute_symbols.hpp"
#include "jit_utils.hpp"
#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

#include <memory>
#include <string>
#include <unordered_set>

LLVM_INCLUDE_BEGIN()
#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
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
 * @brief creates map: opcode -> mangled name.
 */
std::unordered_map<vm::low::MicroOpcode, std::string> createOpcodeNameMap(llvm::Module& module) {
	std::unordered_map<vm::low::MicroOpcode, std::string> lfunc_name_map;

	for (auto& f_box: module.functions()) {
		auto& f = *f_box;
		if (!f.isDeclaration()) {
			auto func_name = f.getName().str();
			auto demangled = llvm::demangle(func_name);
			if (demangled.starts_with("vm::OpFuns::op_")
			    and !demangled.starts_with("vm::OpFuns::op_debug")) {
				auto name              = extractFunctionName(demangled);
				name                   = name.substr(3);  // delete op_
				auto opcode            = vm::low::getOpcode(name);
				lfunc_name_map[opcode] = func_name;
			}
		}
	}
	CORE_ASSERT(!lfunc_name_map.empty(), "Opfuns not found!");
	return lfunc_name_map;
}

/**
 * @brief Clones module corresponding to each opcodes and one with global data
 * to preserve them for future cloning.
 */
void cloneOpcodesModules(
	const std::unordered_set<std::string>& name_set,
	llvm::Module&                          module,
	llvm::orc::LLJIT&                      lljit_instance,
	llvm::orc::ThreadSafeContext&          g_context,
	llvm::ExitOnError&                     exit_on_err
) {
	auto globals_filter = [&](const llvm::GlobalValue* gv) -> bool {
		return !name_set.contains(gv->getName().str());
	};
	cloneAndRegisterModule(module, lljit_instance, globals_filter, g_context, exit_on_err);

	for (const auto& opfun_name: name_set) {
		auto opfun_filter = [&](const llvm::GlobalValue* gv) -> bool {
			return gv->getName().str() == opfun_name;
		};

		cloneAndRegisterModule(module, lljit_instance, opfun_filter, g_context, exit_on_err);
	}
}

/**
 * @brief Finds or creates LLVM types used in opcode function definitions and returns them in a
 * struct.
 */
LlvmData::LlvmTypes findOrCreateTypes(std::unique_ptr<llvm::orc::ThreadSafeContext>& g_context) {
	// Casting to Ref is used to detect nullptr.
	auto flag_data_ty
		= Ref(llvm::StructType::create(*g_context->getContext(), "struct.vm::FlagData"));
	flag_data_ty->setBody(
		{
			llvm::IntegerType::get(*g_context->getContext(), 1)  // bool flag
		},
		/*isPacked=*/false
	);

	auto frame_ty = Ref(llvm::StructType::create(*g_context->getContext(), "struct.vm::Frame"));
	frame_ty->setBody(
		{ flag_data_ty.get() },
		/*isPacked=*/false
	);

	auto microinstruction_ty = Ref(llvm::StructType::create(
		*g_context->getContext(),
		{
			// Layout of microinstructions struct in switch case version.
			llvm::Type::getInt64Ty(*g_context->getContext()),  // opcode
			llvm::Type::getInt64Ty(*g_context->getContext()),  // arg0
			llvm::Type::getInt64Ty(*g_context->getContext())   // arg1
		},
		"vm::MicroInstruction"
	));

	auto vm_thread_ty = Ref(llvm::StructType::create(*g_context->getContext(), "vm::VMThread"));

	auto mi_ptr_ptr_ty = Ref(llvm::PointerType::getUnqual(
		Ref(llvm::PointerType::getUnqual(microinstruction_ty.get())).get()
	));

	auto byte_ptr_ptr_ty = Ref(llvm::PointerType::getUnqual(
		Ref(llvm::PointerType::getUnqual(Ref(llvm::Type::getInt8Ty(*g_context->getContext())).get()))
			.get()
	));

	auto frame_ptr_ptr_ty
		= Ref(llvm::PointerType::getUnqual(Ref(llvm::PointerType::getUnqual(frame_ty.get())).get()));
	auto vm_thread_ptr_ty = Ref(llvm::PointerType::getUnqual(vm_thread_ty.get()));

	auto opfun_ty = Ref(llvm::FunctionType::get(
		Ref(llvm::Type::getVoidTy(*g_context->getContext())).get(),
		{ mi_ptr_ptr_ty.get(), byte_ptr_ptr_ty.get(), frame_ptr_ptr_ty.get(), vm_thread_ptr_ty.get() },
		false
	));

	return LlvmData::LlvmTypes{ .frame            = frame_ty,
		                        .flag_data        = flag_data_ty,
		                        .microinstruction = microinstruction_ty,
		                        .vm_thread        = vm_thread_ty,
		                        .opfun            = opfun_ty };
}

LlvmData initLlvmJit() {
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	auto initial_context = std::make_unique<llvm::LLVMContext>();

	auto              g_module = parseOpcodesBitcode(*initial_context);
	llvm::ExitOnError exit_on_err;

	auto  lljit_instance = exit_on_err(llvm::orc::LLJITBuilder().create());
	auto& jd             = lljit_instance->getMainJITDylib();
	jd.addGenerator(cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
		lljit_instance->getDataLayout().getGlobalPrefix()
	)));

	registerAbsoluteJITSymbols(*lljit_instance);

	CORE_ASSERT(g_module->isMaterialized(), "Opfuns module not fully materialized!");
	externalizeAllGlobalValues(*g_module);

	auto opcode_name_map = createOpcodeNameMap(*g_module);

	std::unordered_set<std::string> name_set;  // Hashset of all mangled names for fast lookup.
	for (const auto& [k, v]: opcode_name_map) name_set.insert(v);

	auto g_context = std::make_unique<llvm::orc::ThreadSafeContext>(std::move(initial_context));

	cloneOpcodesModules(name_set, *g_module, *lljit_instance, *g_context, exit_on_err);

	auto types = findOrCreateTypes(g_context);

	return LlvmData{ .g_context       = std::move(g_context),
		             .g_module        = std::move(g_module),
		             .lljit_instance  = std::move(lljit_instance),
		             .exit_on_err     = std::move(exit_on_err),
		             .opcode_name_map = std::move(opcode_name_map),
		             .types           = types };
}

const LlvmData& llvmData() {
	// @TODO: #2862 Find a suitable place to store LlvmData instead of making it static.
	static LlvmData llvm_data = initLlvmJit();
	return llvm_data;
}

base::Optional<std::string_view> LlvmData::getFunName(const vm::low::MicroOpcode& fun) const {
	auto fun_name_iter = opcode_name_map.find(fun);
	if (fun_name_iter != opcode_name_map.end()) return fun_name_iter->second;
	return {};
}
