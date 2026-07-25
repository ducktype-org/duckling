#include "ivm_compiler.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>
#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>
#include <vm/core/builtin_functions.hpp>

#include <ranges>

namespace vm::loader::compiler {

	detail::FunctionStackContext IVMCompiler::calculateStackContext(
		const code::valid_function::ValidFunction& function
	) const {
		detail::FunctionStackContext ctx(function);

		code::valid_type::TypeSize max_stack_size{ Bytes{ 0 }, Bytes{ 0 } };
		usize                      max_block_count = 0;

		auto& db = ctx.function.local_stack;

		for (const auto& state: ctx.function.stack_states) {
			max_block_count = std::max(max_block_count, db.size(state));
			max_stack_size  = max_stack_size.fieldMax(db.byteSize(state));
		}

		ctx.local_stack_size  = max_stack_size;
		ctx.local_block_count = max_block_count;

		return ctx;
	}

	IVMCompiler::IVMCompiler(const code::ValidProgram& high_program): high_program(high_program) {
		// @note We can't call recompile here, because it calls a virtual function,
		// and if we want to allow subclasses to call this constructor, the virtual function will be
		// called before the subclass constructor is executed, which leads to undefined behavior.
		// So every subclass constructor has to call recompile explicitly after calling this constructor.
	}

	void IVMCompiler::recompile() {
		using namespace std::views;
		using std::ranges::to;

		ProgramSize sizes = getCurrentProgramSize();

		auto new_types = high_program.getTypeContext().getCurrentTypes() | drop(sizes.type_count)
		               | to<std::vector>();
		compileNewTypes(new_types);

		auto new_c_functions
			= high_program.extCFunctions() | drop(sizes.ext_c_function_count) | to<std::vector>();
		compileNewExtCFunctions(new_c_functions);

		auto new_ffi_functions
			= high_program.ffiFunctions() | drop(sizes.ffi_function_count) | to<std::vector>();
		compileNewFFIFunctions(new_ffi_functions);

		auto new_globals = high_program.globals() | drop(sizes.global_count) | to<std::vector>();

		compileNewGlobals(new_globals);

		auto to_cref
			= []<typename T>(const SharedBox<T>& shared_box) -> CRef<T> { return &(*shared_box); };

		auto new_functions = high_program.functions() | drop(sizes.function_count)
		                   | transform(to_cref) | to<std::vector>();

		compileNewFunctions(new_functions);
	}
}
