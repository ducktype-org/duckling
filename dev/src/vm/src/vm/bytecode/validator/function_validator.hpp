#pragma once

#include "flag_context.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_function.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>

namespace vm::code::detail {

	struct Normal {};

	struct Expr {
		CRef<SafeVMThread> thread;
	};

	/**
	 * @brief Validation mode for the synthetic `vm_start_function` wrappers. Behaves like `Expr`
	 * (needs the live thread to resolve `init_pany_vmval` arguments) but additionally allows
	 * `exit` and starts the local stack at the base of the frame instead of the current stack.
	 */
	struct StartFunction {
		CRef<SafeVMThread> thread;
	};

	using ValidationMode = std::variant<Normal, Expr, StartFunction>;

	/**
	 * @brief Performs function code validation in the given context and extracts reachable code.
	 */
	valid_function::ValidFunction validateAndExtractReachableCode(
		const valid_type::ValidTypeMap&                  types,
		const ObjIdNameMap<GlobalData>&                  globals_map,
		const base::HashMap<base::StrID, FuncSignature>& signatures,
		const ObjIdNameMap<ExternalCFunction>&           ext_c_functions,
		const FlagContext&                               flag_context,
		const ObjIdNameMap<FFIFunction>&                 ffi_functions,
		const Function&                                  function,
		ValidationMode                                   mode = Normal{}
	);
}
