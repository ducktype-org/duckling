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


#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
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
		DestroyCV,
		CptrRead,
		CptrWrite
	};

	/**
	 * @brief Placeholder parameter type used for a builtin argument whose type is checked by the
	 * builtin's own `arg_verifier` rather than by exact name match. It only contributes to the
	 * parameter count; it is never resolved as a real type.
	 */
	inline constexpr std::string_view VERIFIER_CHECKED_PARAM = "$checked";

	/**
	 * @brief Verifies the argument types of a builtin call that cannot be expressed as a fixed
	 * list of type names (e.g. a pointer to any type). Returns an error message if the types are
	 * invalid, or an empty optional if they are acceptable.
	 * @param arg_types The concrete types of the arguments on the stack, in call order.
	 */
	using BuiltinArgVerifier = base::Optional<std::string> (*)(
		const std::vector<base::CRef<code::valid_type::ValidType>>& arg_types
	);

	/**
	 * @brief Full description of a builtin function: its bytecode-visible name, its signature,
	 * and an optional custom argument verifier.
	 */
	struct BuiltinFunction {
		base::StrID         name;
		code::FuncSignature signature;
		/// When set, validates the whole argument list instead of exact per-parameter type-name
		/// matching. Parameters it covers use `VERIFIER_CHECKED_PARAM` as a placeholder.
		BuiltinArgVerifier arg_verifier = nullptr;

		BuiltinFunction(
			base::StrID         name,
			code::FuncSignature signature,
			BuiltinArgVerifier  arg_verifier = nullptr
		):
			  name(name),
			  signature(std::move(signature)),
			  arg_verifier(arg_verifier) {}
	};

	/**
	 * @brief Returns the argument verifier for a builtin, or nullptr if its arguments are checked
	 * by ordinary exact type-name matching.
	 */
	BuiltinArgVerifier getBuiltinArgVerifier(BuiltinFunctionID id);

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

		/**
		 * @brief Copies the whole block pointed to by the VM pointer `dst` out of the raw C memory
		 * addressed by `src` (a `cptr`). Used to read FFI results back into the VM.
		 * @warning `src` must address at least the block's size of valid, readable memory.
		 */
		static void builtinCptrRead(SafeVMThread& process, u64 src, Pointer dst);

		/**
		 * @brief Copies the whole block pointed to by the VM pointer `src` into the raw C memory
		 * addressed by `dst` (a `cptr`). Used to hand VM data to FFI functions.
		 * @warning `dst` must address at least the block's size of valid, writable memory.
		 */
		static void builtinCptrWrite(SafeVMThread& process, u64 dst, Pointer src);
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
