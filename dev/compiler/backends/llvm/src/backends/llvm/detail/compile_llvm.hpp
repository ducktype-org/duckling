#pragma once

#include <base/string_id.hpp>
#include <base/ref.hpp>
#include <base/optional.hpp>

#include "../llvm_backend.hpp"

namespace compiler::backend_llvm {
	struct ModuleImpl;

	void
		compileModuleToObject(Ref<ModuleImpl> module_impl, base::StrID output_file, CompilationOutputType output_type);
}
