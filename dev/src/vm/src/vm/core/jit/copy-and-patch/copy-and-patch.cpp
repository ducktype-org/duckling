#include <vm/core/thread/low_program/instruction.hpp>

constexpr char binary_stensils[] = {
#embed "stensils/wrapper" \
    suffix(,)
};

struct LLVM_nm_data {
	const char* name;
	const char* type;
	int   place;
	int   size;
};

LLVM_nm_data stensils_offsets[] {
#include "stensils/wrapper.nm"
};


vm::JitOpFun* compile_cp(const vm::low::LowFuncData& func_data) {
    return nullptr;
}