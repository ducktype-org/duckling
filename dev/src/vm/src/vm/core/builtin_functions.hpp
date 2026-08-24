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
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/safe_vmthread.hpp>

namespace vm {
	class SafeVMValue;
	class SafeVMProcess;
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
	enum class BuiltinFunctionID : usize {
		Abort,
		InputI64,
		InputChar,
		OutputI64,
		OutputI32,
		OutputChar,
		OutputString,
		FloatToString,
		U64ToString,
		I64ToString,
		Stoi,
		Strtod,
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
	 * @brief Full description of a builtin function: its bytecode-visible name and its signature.
	 */
	struct BuiltinFunction {
		base::StrID         name;
		code::FuncSignature signature;

		BuiltinFunction(base::StrID name, code::FuncSignature signature):
			  name(name),
			  signature(std::move(signature)) {}
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

		/**
		 * @brief Reads a single raw byte from input (no whitespace skipping), returning it as an
		 * `i32`, or `-1` at end of input (matching libc `getchar`). Backs `core.io.readCharCode`
		 * on the DVM.
		 */
		static i32  builtinInputChar(SafeVMThread& process);
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

		static i64 builtinStoi(SafeVMThread& process, Pointer ptr);

		/**
		 * @brief Parses the leading floating-point number out of the NUL-terminated char table
		 * under `ptr`. Backs `core.io.strtod` on the DVM; the native backend uses libc `strtod`
		 * directly (see `core.clib`).
		 */
		static f64  builtinStrtod(SafeVMThread& process, Pointer ptr);
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
	base::Optional<Box<SafeVMValue>> callBuiltinFunction(
		BuiltinFunctionID                    id,
		const std::vector<TypeCRef>&         result_types,
		SafeVMProcess&                       process,
		SafeVMThread&                        thread,
		const std::vector<Box<SafeVMValue>>& arguments
	);

	/**
	 * @brief Returns the map of builtin functions with lazy initialization.
	 * @note Function types here should match HELIOS types.
	 * The types used for the parameters and the return value are defined in the @file
	 * bytecode/builtin_types.hpp file (like "i64", "i32").
	 */
	auto getBuiltinFunctions() -> CRef<std::unordered_map<BuiltinFunctionID, BuiltinFunction>>;

	inline CRef<code::FuncSignature> getBuiltinFunctionSignature(BuiltinFunctionID id) {
		return &getBuiltinFunctions()->at(id).signature;
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
