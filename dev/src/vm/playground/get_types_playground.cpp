
#include <vm/api/api.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/extern_cpp_function.hpp>

#include <iostream>
#include "base/type_traits.hpp"
#include "init/init.hpp"

DEF_VM_EXT_CPP_FUNC(i32, "i32", add, (i32, "i32", a), (i32, "i32", b)) { return a + b; }

DEF_VM_EXT_CPP_FUNC(i32, "i32", add2, (i32*, "ptr_i32", a), (i32*, "ptr_i32", b)) { return *a + *b; }

DEF_VM_EXT_CPP_FUNC(i64, "i64", square, (i64, "i64", base)) { return base * base; }

int main() {
	init::InitObject _;

	auto functions = {
		VM_INSTANCE_EXT_CPP_FUNC(add),
		VM_INSTANCE_EXT_CPP_FUNC(add2),
		VM_INSTANCE_EXT_CPP_FUNC(square),
	};

	auto data = new std::byte[1'000];

	vm::safeWriteBytes(data, (i32) 7);
	vm::safeWriteBytes(data + sizeof(i32), (i32) 10);
	auto result = function_pointer(data);
	std::cout << "Result1: " << *reinterpret_cast<add::Result*>(result.refMut().get()) << "\n";

	auto signature = add2::getSignature();
	std::cout << signature.result_type.str.str() << '\n';
	for (const auto& param: signature.parameters) std::cout << param.str.str() << '\n';

	delete[] data;
}
