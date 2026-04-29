#include "llvm.hpp"

pg::LlvmProgramModule::LlvmProgramModule():
	  context(),
	  module(std::make_unique<llvm::Module>("psackling", context)) {
	initializeTargetMachine();
	module->setTargetTriple(target_machine->getTargetTriple().getTriple());
	module->setDataLayout(target_machine->createDataLayout());
}

void pg::LlvmProgramModule::inCtxDo(const std::function<void(llvm::LLVMContext&, llvm::Module&)>& func) {
	func(context, *module);
}


void pg::LlvmProgramModule::buildMainReturningZero() {
	auto* i32_type = llvm::Type::getInt32Ty(context);
	auto* main_ty  = llvm::FunctionType::get(i32_type, false);
	auto* main_fn
		= llvm::Function::Create(main_ty, llvm::Function::ExternalLinkage, "main", module.get());

	auto*             entry_block = llvm::BasicBlock::Create(context, "entry", main_fn);
	llvm::IRBuilder<> builder(entry_block);
	builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
}

void pg::LlvmProgramModule::verify() const {
	std::string              verify_output;
	llvm::raw_string_ostream verify_stream(verify_output);
	if (llvm::verifyModule(*module, &verify_stream))
		CORE_PANIC("Generated LLVM module failed verification: " + verify_output);
}

void pg::LlvmProgramModule::emitObjectFile(const std::filesystem::path& output_path) {
	std::error_code      error_code;
	llvm::raw_fd_ostream output_stream(output_path.native(), error_code, llvm::sys::fs::OF_None);
	if (error_code) CORE_PANIC("LLVM error: unable to create file: " + error_code.message());

	llvm::legacy::PassManager pass;
	if (target_machine->addPassesToEmitFile(
			pass, output_stream, nullptr, llvm::CodeGenFileType::ObjectFile
		)) {
		CORE_PANIC("TargetMachine can't emit object file");
	}

	pass.run(*module);
	output_stream.flush();
}

void pg::LlvmProgramModule::initializeTargetMachine() {
	if (llvm::InitializeNativeTarget())
		CORE_PANIC("LLVM error: failed to initialize native target");
	if (llvm::InitializeNativeTargetAsmPrinter())
		CORE_PANIC("LLVM error: failed to initialize native target asm printer");

	const llvm::Triple target_triple(llvm::sys::getDefaultTargetTriple());
	std::string        target_error;
	auto* target = llvm::TargetRegistry::lookupTarget(target_triple.getTriple(), target_error);
	if (!target) CORE_PANIC("LLVM error: " + target_error);

	llvm::TargetOptions options;
	target_machine.reset(target->createTargetMachine(
		target_triple.getTriple(),
		"generic",
		"",
		options,
		llvm::Reloc::PIC_,
		std::nullopt,
		llvm::CodeGenOptLevel::None
	));
	if (!target_machine) CORE_PANIC("LLVM error: failed to create target machine");
}
