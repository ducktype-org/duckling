#pragma once

#include "psackling/ids.hpp"
#include "psackling/llvm.hpp"

#include "base/collections/maps.hpp"

namespace pg {
	class Context;

	class LlvmLoweringContext {
	public:
		LlvmLoweringContext(LlvmProgramModule& llvm_module): llvm_module(llvm_module) {}

		llvm::Type* lowerType(tp::TypeID type_id, Context& ctx);

		/// Returns the llvm::Function for a FunctionSymbol, creating a declaration on
		/// first use (so calls may reference functions defined later in the file).
		llvm::Function* getOrCreateFunction(sym::SymbolID fun_sym_id, Context& ctx);

	private:
		LlvmProgramModule& llvm_module;

		base::HashMap<tp::TypeID, llvm::Type*>        types;
		base::HashMap<sym::SymbolID, llvm::Function*> functions;
	};
}
