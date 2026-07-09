/**
 * @file builtin_functions.hpp
 * @author Wojciech Rzepliński
 * @brief The implementation of the builtin functions in the VM.
 *
 * The builtin functions have custom C++ implementation that can interact with the outside world
 * but also with the VM's thread and process (like set thread status to "waitingForInput").
 *
 * This module has two separate parts:
 * - low level implementations, handling the VMThread calls to the builtin functions,
     and compiling the call_builtinfunc opcode
 * - high level builtin "stdlib" module with DBC code to better interact with the loader
 *
 * The high level builtin functions serves as wrappers for the low level "call_builtinfunc" opcodes.
 * Other DBC programs should just call the stdlib functions the same way as any other function.
 *
 * For now both the high and low level builtins use the same prototypes.
 *
 * The goal of this implementation is to have one source file for the builtin functions -
 * this file. They have to be consistent with the HELIOS builtin list and LLVM builtins manually.
 *
 * @warning The verification of the call_builtinfunc opcode is not decided yet.
 */
#pragma once


#include <base/types/floats.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>

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
	enum class BuiltinFunctionID : usize {
		Abort,
		InputI64,
		OutputI64,
		OutputI32,
		OutputChar,
		OutputString,
		FloatToString,
		U64ToString,
		I64ToString,
		Stoi,
		StartThread,
		JoinThread,
		CreateMutex,
		LockMutex,
		UnlockMutex,
		DestroyMutex,
		CreateCV,
		WaitCV,
		NotifyCV,
		NotifyAllCV,
		DestroyCV
	};

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
		static void builtinAbort(SafeVMThread& process);
		static i64  builtinInputI64(SafeVMThread& process);
		static i64  builtinOutputI64(SafeVMThread& process, i64 arg);
		static i64  builtinOutputI32(SafeVMThread& process, i32 arg);
		static i64  builtinOutputChar(SafeVMThread& process, i8 arg);
		static void builtinOutputString(SafeVMThread& process, Pointer ptr);

		/**
		 * @brief Number formatting into the char table of `buffer_cap` bytes under `ptr`.
		 *
		 * Each one writes the decimal representation of the value followed by a terminating
		 * NUL and returns how many characters it wrote (excluding the NUL), or `0` when the
		 * representation plus its NUL does not fit into `buffer_cap` bytes. These back
		 * `core.runtime` and must stay in sync with the native builtins of the same names
		 * (see `builtins_source.cpp`).
		 */
		static u64 builtinFloatToString(
			SafeVMThread& process, f64 value, Pointer ptr, u64 buffer_cap
		);
		static u64 builtinU64ToString(SafeVMThread& process, u64 value, Pointer ptr, u64 buffer_cap);
		static u64 builtinI64ToString(SafeVMThread& process, i64 value, Pointer ptr, u64 buffer_cap);

		static i64  builtinStoi(SafeVMThread& process, Pointer ptr);
		static i64  builtinStartThread(SafeVMThread& process);
		static i64  builtinJoinThread(SafeVMThread& process, u64 thread_id);
		static u64  builtinCreateMutex(SafeVMThread& process);
		static void builtinLockMutex(SafeVMThread& process, u64 mutex_id);
		static void builtinUnlockMutex(SafeVMThread& process, u64 mutex_id);
		static void builtinDestroyMutex(SafeVMThread& process, u64 mutex_id);
		static u64  builtinCreateCV(SafeVMThread& process);
		static void builtinWaitCV(SafeVMThread& process, u64 cv_id, u64 mutex_id);
		static void builtinNotifyCV(SafeVMThread& process, u64 cv_id);
		static void builtinNotifyAllCV(SafeVMThread& process, u64 cv_id);
		static void builtinDestroyCV(SafeVMThread& process, u64 cv_id);
	};

	/**
	 * @brief Calls a builtin function with the given ID and arguments.
	 */
	base::Optional<Box<VmValue>> callBuiltinFunction(
		BuiltinFunctionID                id,
		const std::vector<TypeCRef>&     result_types,
		IVMProcess&                      process,
		SafeVMThread&                    thread,
		const std::vector<Box<VmValue>>& arguments
	);

	/**
	 * @brief Returns the map of builtin functions types with lazy initialization.
	 * @note Function types here should match HELIOS types.
	 * The types used for the parameters and the return value are defined in the @file
	 * bytecode/builtin_types.hpp file (like "i64", "i32").
	 */
	auto getBuiltinFunctions()
		-> CRef<std::unordered_map<BuiltinFunctionID, std::pair<base::StrID, code::FuncSignature>>>;

	inline CRef<code::FuncSignature> getBuiltinFunctionSignature(BuiltinFunctionID id) {
		return &getBuiltinFunctions()->at(id).second;
	}

	base::Optional<CRef<code::FuncSignature>> getBuiltinFunctionSignature(base::StrID name);

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
}
