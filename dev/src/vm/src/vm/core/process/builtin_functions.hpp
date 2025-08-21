/**
 * @file builtin_functions.hpp
 * @author Wojciech Rzepliński
 * @brief The implemention of the builtin functions in the VM.
 *
 * The builtin functions have custom C++ implementation that can interact with the outside world
 * but also with the VM's thread and process (like set thread status to "waitingForInput").
 *
 * This module has two seperate parts:
 * - low level implementations, handling the VMThread calls to the builtin functions,
     and compiling the call_builtin_func opcode
 * - high level builtin "stdlib" module with DBC code to better interact with the loader
 *
 * The high level builtin functions serves as wrappers for the low level "call_builtin_func" opcodes.
 * Other DBC programs should just call the stdlib functions the same way as any other function.
 *
 * For now both the high and low level builtins use the same prototypes.
 *
 * The goal of this implementation is to have one source file for the builtin functions -
 * this file. They have to be consistent with the HELIOS builtin list and LLVM builtins manually.
 *
 * @warning The verification of the call_builtin_func opcode is not decided yet.
 */
#pragma once


#include <base/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/thread/vmvalue.hpp>

namespace vm {
	class VMThread;
}

namespace vm::builtins {

	struct NoValue {};

	// Because VMThread stores values on the local stack as bytes,
	// there was a question of where the conversion from bytes to
	// value should be done. I decided that the conversion should be
	// in the VMThread. So the handlers only see the converted values.
	using Value = std::variant<i64, NoValue>;

	/**
	 * @note Name of the enum case should be the same as the builtin function name
	 * without the "builtin" prefix.
	 */
	enum class BuiltinFunctionID : usize { InputI64, OutputI64 };

	/**
	 * @brief Class for FunctionHandlers.
	 *
	 * Each handler should be defined as static and have @p FunctionHandler type.
	 * The only reason this class exists is to enable the VMThread class to
	 * be a friend of the FunctionHandlers class, so the handlers can access
	 * the VMThread private members.
	 */
	class FunctionHandlers {
	public:
		static i64 builtinInputI64(VMThread& process);
		static i64 builtinOutputI64(VMThread& process, i64 arg);
	};

	/**
	 * @brief Calls a builtin function with the given ID and arguments.
	 */
	base::Optional<Box<VmValue>> callBuiltinFunction(
		BuiltinFunctionID                id,
		TypeCRef                         result_type,
		VMProcess&                       process,
		VMThread&                        thread,
		const std::vector<Box<VmValue>>& arguments
	);

	/**
	 * @brief Returns the map of builtin functions types with lazy initialization.
	 * @note Function types here should match HELIOS types.
	 * The types used for the parameters and the return value are defined in the @file
	 * bytecode/builtin_types.hpp file (like "i64", "i32", "void").
	 */
	auto getBuiltinFunctions()
		-> CRef<std::unordered_map<BuiltinFunctionID, std::pair<base::StrID, code::FuncSignature>>>;

	inline CRef<code::FuncSignature> getBuiltinFunctionSignature(BuiltinFunctionID id) {
		return &getBuiltinFunctions()->at(id).second;
	}

	/**
	 * @brief Get the ID of the builtin function given the name.
	 * ID is the index in the BUILTIN_FUNCTIONS array.
	 *
	 * The ID is used in the LowVMProgram to store opcode
	 * arguments as numerical values (not strings).
	 */
	base::Optional<BuiltinFunctionID> getBuiltinFunctionID(base::StrID name);

	/**
	 * @brief Returns true if the name is a builtin function name.
	 */
	bool isBuiltinFunction(base::StrID name);

	/**
	 * @brief Get the stdlib module with the builtin functions.
	 * The builtin functions are regular functions that have simple implementation
	 * - they call the "real" builtin function with `call_builtin_func`.
	 * But thanks to having these these wrappers,
	 * user can call builtins with simple `call_func` opcode.
	 *
	 * @note Both the wrapper and real builtin use the same function types.
	 * @note Function prototypes depend on the builtin types.
	 *
	 * The module with all the functions is generated on the first use of this function.
	 *
	 * @return Ref<code::CodeCollection>
	 */
	CRef<code::CodeCollection> getStdlibModule();
}
