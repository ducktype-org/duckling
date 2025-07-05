#include "operations.hpp"

#include "backend_stuff/llvm_ir_lib.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <global_state/artifacts_location.hpp>

namespace compiler::driver {


	artifacts::FileArtifact emitBuiltinLLVMObjectFile() {
		auto builtin_obj_file
			= global_state::getRootCollection()->fileArtifactAtOrNew(base::StrID("builtins_llvm.o"));
		auto mod = backend_llvm::Module::fromIRCode(LLVM_IR_LIB);
		mod.compile(builtin_obj_file.FILE, backend_llvm::CompilationOutputType::Object);
		return builtin_obj_file;
	}
}
