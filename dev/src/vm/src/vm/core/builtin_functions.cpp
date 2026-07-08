#include "builtin_functions.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/safe/concurrency/synchronization_primitives.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vm::builtins {

	namespace {
		template<class Ret, class... FunArgs, std::size_t... Is>
		base::Optional<Box<VmValue>>
			callUnpackArgsImpl(Ret (*function)(SafeVMThread&, FunArgs...), const std::vector<TypeCRef>& vm_return_types, IVMProcess& process, SafeVMThread& thread, const std::vector<Box<VmValue>>& args, std::index_sequence<Is...>) {
			if constexpr (std::is_void_v<Ret>) {
				function(thread, args[Is]->template readBytes<FunArgs>()...);
				return {};
			} else {
				CORE_ASSERT(
					vm_return_types.size() == 1, "Builtin Function with more than 1 return value"
				);
				auto value = function(thread, args[Is]->template readBytes<FunArgs>()...);
				CORE_ASSERT(
					sizeof(value) == vm_return_types.at(0)->getSize().asInt(),
					"Type sizes do not match"
				);

				auto vm_value = process.createOwnedVmValue(vm_return_types.at(0));

				vm_value->writeBytes<Ret>(value);
				return vm_value;
			}
		}

		/**
		 * @brief Helper function that unpacks the arguments from the vector,
		 * converts them to the required types and calls the function.
		 * Throws an exception if the types do not match.
		 *
		 * Similar to python '*' operator:
		 * @code
		 * args = [1, 2, 4]
		 * f(*args)
		 *
		 * It checks if the size of the args vector is the same as the number of parameters.
		 * It checks if each variant type is the same as the function parameter type.
		 *
		 * @note alternative would be to pass the vector of Values to each handler and perform the
		 * `std::get` on variants inside each handler. Maybe if copying the values in calls would
		 * be expensive we could go back to that approach.
		 */
		template<class Ret, class... FunArgs>
		base::Optional<Box<VmValue>> callUnpackArgs(
			Ret (*function)(SafeVMThread&, FunArgs...),
			const std::vector<TypeCRef>&     vm_return_types,
			IVMProcess&                      process,
			SafeVMThread&                    thread,
			const std::vector<Box<VmValue>>& args
		) {
			CORE_ASSERT(
				sizeof...(FunArgs) == args.size(),
				"Wrong number of arguments passed to the builtin function"
			);
			return callUnpackArgsImpl(
				function,
				vm_return_types,
				process,
				thread,
				args,
				std::index_sequence_for<FunArgs...>{}
			);
		}

		/**
		 * @brief Formats `value` with `format` directly into the char table under `ptr`,
		 * whose total capacity is `buffer_cap` bytes. Writes at most `buffer_cap - 1`
		 * characters followed by a terminating NUL, as `std::snprintf` does. Returns the
		 * number of characters written (excluding the NUL), or `0` when the representation
		 * plus its NUL does not fit.
		 *
		 * The span is bounds-checked, so a caller lying about the capacity throws instead of
		 * corrupting the heap. On a non-fit the buffer may hold a truncated result, so
		 * callers must ignore it when `0` is returned.
		 */
		template<typename T>
		u64 writeFormatted(Pointer ptr, u64 buffer_cap, const char* format, T value) {
			auto        destination = Memory::getPointerData(ptr, buffer_cap);
			auto* const buffer      = reinterpret_cast<char*>(destination.getBegin());
			const int   written     = std::snprintf(buffer, buffer_cap, format, value);  // NOLINT
			CORE_ASSERT(written > 0, "Formatting a number into a string failed");

			const auto length = base::safeIntConv<u64>(written);
			if (length >= buffer_cap) return 0;
			return length;
		}
	}

	// ============================== BUILTIN IMPLEMENTATIONS ==============================

	void FunctionHandlers::builtinAbort(SafeVMThread& /*thread*/) {
		throw vm::exceptions::VMPanicException{};
	}

	i64 FunctionHandlers::builtinInputI64(SafeVMThread& thread) {
		thread.setProcessStatus(api::Sleeping{});
		auto return_value = thread.safe_process.getIO().getInput<i64>(thread);
		thread.setProcessStatus(api::Running{});
		return return_value;
	}

	i32 FunctionHandlers::builtinInputChar(SafeVMThread& thread) {
		thread.setProcessStatus(api::Sleeping{});
		const int c = thread.safe_process.getIO().getRawChar(thread);
		thread.setProcessStatus(api::Running{});
		return static_cast<i32>(c);
	}

	i64 FunctionHandlers::builtinOutputI64(SafeVMThread& thread, i64 arg) {
		const std::string output = std::to_string(arg) + "\n";
		thread.safe_process.getIO().writeOutput(output);

		return base::safeIntConv<i64>(output.size());
	}

	i64 FunctionHandlers::builtinOutputI32(SafeVMThread& thread, i32 arg) {
		const std::string output = std::to_string(arg) + "\n";
		thread.safe_process.getIO().writeOutput(output);

		return base::safeIntConv<i64>(output.size());
	}

	i64 FunctionHandlers::builtinOutputChar(SafeVMThread& thread, i8 arg) {
		std::string output(1, static_cast<char>(arg));
		thread.safe_process.getIO().writeOutput(output);

		return base::safeIntConv<i64>(output.size());
	}

	void FunctionHandlers::builtinOutputString(SafeVMThread& thread, Pointer ptr) {
		auto block      = ptr.getBlock();
		auto block_id   = thread.process_memory.requestBlockID(block);
		auto block_data = thread.process_memory.requestBlockData(block_id);
		auto str_data   = block_data.stdString();
		thread.safe_process.getIO().writeOutput(str_data.substr(0, str_data.size() - 1) + "\n");
	}

	u64 FunctionHandlers::builtinFloatToString(
		SafeVMThread& /*thread*/, f64 value, Pointer ptr, u64 buffer_cap
	) {
		return writeFormatted(ptr, buffer_cap, "%g", double(value));
	}

	u64 FunctionHandlers::builtinU64ToString(
		SafeVMThread& /*thread*/, u64 value, Pointer ptr, u64 buffer_cap
	) {
		return writeFormatted(ptr, buffer_cap, "%lu", value);
	}

	u64 FunctionHandlers::builtinI64ToString(
		SafeVMThread& /*thread*/, i64 value, Pointer ptr, u64 buffer_cap
	) {
		return writeFormatted(ptr, buffer_cap, "%ld", value);
	}

	i64 FunctionHandlers::builtinStoi(SafeVMThread& thread, Pointer ptr) {
		auto block      = ptr.getBlock();
		auto block_id   = thread.process_memory.requestBlockID(block);
		auto block_data = thread.process_memory.requestBlockData(block_id);

		auto str_data = block_data.stdString();
		return std::stoll(str_data);
	}

	f64 FunctionHandlers::builtinStrtod(SafeVMThread& thread, Pointer ptr) {
		auto block      = ptr.getBlock();
		auto block_id   = thread.process_memory.requestBlockID(block);
		auto block_data = thread.process_memory.requestBlockData(block_id);

		// The block spans the whole allocated buffer, which may be larger than the string content.
		// The content is NUL-terminated, so hand the raw pointer to `strtod` directly: it stops at
		// the NUL and never reads the trailing (possibly uninitialized) bytes.
		return std::strtod(reinterpret_cast<const char*>(block_data.getBegin()), nullptr);
	}

	i64 FunctionHandlers::builtinStartThread(SafeVMThread& thread) {
		thread.releaseGil();
		auto result = vm::api::runFunction(thread.safe_process.getPID(), thread.getThreadCtx());
		thread.acquireGil();
		if (!result.has_value()) return -vm::api::errorToErrno(result.error());
		return static_cast<i64>(result.value().asInt());
	}

	i64 FunctionHandlers::builtinJoinThread(SafeVMThread& thread, u64 thread_id) {
		thread.releaseGil();
		auto result = vm::api::join(thread.safe_process.getPID(), api::ThreadID{ thread_id });
		thread.acquireGil();
		if (!result.has_value()) return -vm::api::errorToErrno(result.error());
		return 0;  // success
	}

	u64 FunctionHandlers::builtinCreateMutex(SafeVMThread& thread) {
		return thread.safe_process.getSynchronizationPrimitives().addMutex();
	}

	void FunctionHandlers::builtinLockMutex(SafeVMThread& thread, u64 mutex_id) {
		auto  mutex     = thread.safe_process.getSynchronizationPrimitives().getMutex(mutex_id);
		auto  thread_id = thread.getThreadID();
		auto* detector  = thread.safe_process.getDeadlockDetector();

		// Deadlock check must happen before any acquisition attempt. When enabled and the mutex
		// is free, checkForDeadlock returns immediately (no owner → no cycle possible), so
		// there is no cost on the fast path.
		if_opt_some(detector, d) d.checkForDeadlock(thread_id, mutex_id);

		// Fast path: mutex is free — grab it with a non-blocking CAS while still holding the
		// GIL. Skips the GIL release/acquire pair and the expensive timed syscall. We also
		// skip the intermediate "waiting" state in the detector because we never waited.
		if (mutex->try_lock()) {
			if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
			return;
		}

		// Slow path: mutex is contended. Mark ourselves as waiting, then release the GIL so
		// other DVM threads can run while we block.
		if_opt_some(detector, d) d.markThreadWaitingForMutex(thread_id, mutex_id);
		thread.releaseGil();
		while (!mutex->try_lock_for(std::chrono::milliseconds{ 500 })) {
			if (thread.isTerminateRequested()) {
				// Acquire GIL before throwing: exception handlers and destructors need exclusive
				// access to process state (memory blocks, primitives, thread metadata) during cleanup.
				thread.acquireGil();
				if_opt_some(detector, d) d.markThreadStoppedWaiting(thread_id);
				throw vm::KillProcessException{};
			}
		}
		// Acquire GIL after blocking: bytecode execution requires holding the GIL.
		thread.acquireGil();
		if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
	}

	void FunctionHandlers::builtinUnlockMutex(SafeVMThread& thread, u64 mutex_id) {
		auto mutex = thread.safe_process.getSynchronizationPrimitives().getMutex(mutex_id);

		auto  thread_id = thread.getThreadID();
		auto* detector  = thread.safe_process.getDeadlockDetector();
		mutex->unlock();
		if_opt_some(detector, d) d.markThreadReleasedMutex(thread_id, mutex_id);
	}

	void FunctionHandlers::builtinDestroyMutex(SafeVMThread& thread, u64 mutex_id) {
		auto mutex = thread.safe_process.getSynchronizationPrimitives().getMutex(mutex_id);
		// Destroying a still-held mutex is a program error. try_lock() never blocks and fails iff
		// the mutex is currently locked (by this or any thread), so use it to detect and report
		// the misuse instead of silently destroying a locked mutex (which also aborts at teardown
		// on macOS). On the success path the mutex was free and we now own it, so release it.
		if (!mutex->try_lock()) throw vm::exceptions::VMDestroyLockedMutexException{};
		mutex->unlock();

		if_opt_some(thread.safe_process.getDeadlockDetector(), d) d.clearMutexState(mutex_id);
		thread.safe_process.getSynchronizationPrimitives().removeMutex(mutex_id);
	}

	u64 FunctionHandlers::builtinCreateCV(SafeVMThread& thread) {
		return thread.safe_process.getSynchronizationPrimitives().addCV();
	}

	void FunctionHandlers::builtinWaitCV(SafeVMThread& thread, u64 cv_id, u64 mutex_id) {
		auto  cv        = thread.safe_process.getSynchronizationPrimitives().getCV(cv_id);
		auto  mutex     = thread.safe_process.getSynchronizationPrimitives().getMutex(mutex_id);
		auto  thread_id = thread.getThreadID();
		auto* detector  = thread.safe_process.getDeadlockDetector();

		// cv->wait atomically unlocks the mutex then relocks it before returning (on any path),
		// so we mirror that in the detector: release ownership now, reacquire after the wait.
		if_opt_some(detector, d) d.markThreadReleasedMutex(thread_id, mutex_id);
		thread.releaseGil();
		try {
			const bool interrupted
				= cv->wait(*mutex, [&thread] { return thread.isTerminateRequested(); });
			if (interrupted) throw vm::KillProcessException{};
		} catch (const vm::exceptions::VMRuntimeException&) {
			// Acquire GIL: exception handlers and destructors need exclusive access to process
			// state during cleanup and propagation of VM runtime exceptions.
			thread.acquireGil();
			if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
			throw;
		} catch (const vm::KillProcessException&) {
			// Acquire GIL: exception handlers and destructors need exclusive access to process
			// state.
			thread.acquireGil();
			if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
			throw;
		} catch (const std::exception& e) {
			// Acquire GIL: exception handlers and destructors need exclusive access to process
			// state. Then wrap the exception so the VM can report ExecutionPanicked.
			thread.acquireGil();
			if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
			std::string msg = "builtinWaitCV failed during condition variable wait: ";
			msg += e.what();
			throw vm::exceptions::VMRuntimeException(std::move(msg));
		} catch (...) {
			// Acquire GIL: exception handlers and destructors need exclusive access to process
			// state. Then convert unknown exceptions into a VMRuntimeException.
			thread.acquireGil();
			if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
			throw vm::exceptions::VMRuntimeException(
				"builtinWaitCV failed during condition variable wait with an unknown exception"
			);
		}
		// Acquire GIL after blocking: bytecode execution requires holding the GIL.
		thread.acquireGil();
		if_opt_some(detector, d) d.markThreadAcquiredMutex(thread_id, mutex_id);
	}

	void FunctionHandlers::builtinNotifyCV(SafeVMThread& thread, u64 cv_id) {
		auto cv = thread.safe_process.getSynchronizationPrimitives().getCV(cv_id);
		cv->notifyOne();
	}

	void FunctionHandlers::builtinNotifyAllCV(SafeVMThread& thread, u64 cv_id) {
		auto cv = thread.safe_process.getSynchronizationPrimitives().getCV(cv_id);
		cv->notifyAll();
	}

	void FunctionHandlers::builtinDestroyCV(SafeVMThread& thread, u64 cv_id) {
		thread.safe_process.getSynchronizationPrimitives().removeCV(cv_id);
	}

	base::Optional<Box<VmValue>> callBuiltinFunction(
		BuiltinFunctionID                id,
		const std::vector<TypeCRef>&     result_types,
		IVMProcess&                      process,
		SafeVMThread&                    thread,
		const std::vector<Box<VmValue>>& arguments
	) {
		switch (id) {
#define CASE_FUNC(ID_NAME)                                                               \
	case BuiltinFunctionID::ID_NAME: {                                                   \
		return callUnpackArgs(                                                           \
			FunctionHandlers::builtin##ID_NAME, result_types, process, thread, arguments \
		);                                                                               \
	}

			FOR_EACH(
				CASE_FUNC,
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
			)


		default:
			CORE_PANIC("Invalid builtin function ID");
		}
	}

	auto getBuiltinFunctions() -> CRef<std::unordered_map<BuiltinFunctionID, BuiltinFunction>> {
		static const std::unordered_map<BuiltinFunctionID, BuiltinFunction> map{
			{
				BuiltinFunctionID::Abort,
				{ base::StrID("abort"), code::FuncSignature({}, {}) },
			},
			{
				BuiltinFunctionID::InputI64,
				{ base::StrID("builtin_input_i64"),
			      code::FuncSignature({ base::StrID("i64") }, {}) },
			},
			{
				BuiltinFunctionID::InputChar,
				{ base::StrID("builtin_input_char"),
			      code::FuncSignature({ base::StrID("i32") }, {}) },
			},
			{
				BuiltinFunctionID::OutputI64,
				{ base::StrID("builtin_output_i64"),
			      code::FuncSignature({ base::StrID("i64") }, { base::StrID("i64") }) },
			},
			{
				BuiltinFunctionID::OutputI32,
				{ base::StrID("builtin_output_i32"),
			      code::FuncSignature({ base::StrID("i64") }, { base::StrID("i32") }) },
			},
			{
				BuiltinFunctionID::OutputChar,
				{ base::StrID("builtin_output_char"),
			      code::FuncSignature({ base::StrID("i64") }, { base::StrID("i8") }) },
			},
			{
				BuiltinFunctionID::OutputString,
				{ base::StrID("builtin_strOutput_pptr"),
			      code::FuncSignature({}, { base::StrID("ptr_string") }) },
			},
			{
				BuiltinFunctionID::Stoi,
				{
					base::StrID("builtin_stoi_pptr"),
					code::FuncSignature({ base::StrID("i64") }, { base::StrID("ptr_string") }),
				},
			},
			// `manyptr char` lowers to a pointer to a dynamic table of `i8`; the parsed value is
			// an `f64`.
			{
				BuiltinFunctionID::Strtod,
				{
					base::StrID("strtod"),
					code::FuncSignature({ base::StrID("f64") }, { base::StrID("ptr_dyntable_i8") }),
				},
			},
			// `manyptr char` lowers to a pointer to a dynamic table of `i8`, and both `u64`
			// and the returned length lower to `i64`.
			{
				BuiltinFunctionID::FloatToString,
				{ base::StrID("float_to_string"),
			      code::FuncSignature(
					  { base::StrID("i64") },
					  { base::StrID("f64"), base::StrID("ptr_dyntable_i8"), base::StrID("i64") }
				  ) },
			},
			{
				BuiltinFunctionID::U64ToString,
				{ base::StrID("u64_to_string"),
			      code::FuncSignature(
					  { base::StrID("i64") },
					  { base::StrID("i64"), base::StrID("ptr_dyntable_i8"), base::StrID("i64") }
				  ) },
			},
			{
				BuiltinFunctionID::I64ToString,
				{ base::StrID("i64_to_string"),
			      code::FuncSignature(
					  { base::StrID("i64") },
					  { base::StrID("i64"), base::StrID("ptr_dyntable_i8"), base::StrID("i64") }
				  ) },
			},
			{
				BuiltinFunctionID::StartThread,
				{ base::StrID("builtin_start_thread"),
			      code::FuncSignature({ base::StrID("i64") }, {}) },
			},
			{
				BuiltinFunctionID::JoinThread,
				{ base::StrID("builtin_join_thread"),
			      code::FuncSignature({ base::StrID("i64") }, { base::StrID("i64") }) },
			},
			{ BuiltinFunctionID::CreateMutex,
			  { base::StrID("builtin_create_mutex"),
			    code::FuncSignature({ base::StrID("mutex") }, {}) } },
			{ BuiltinFunctionID::LockMutex,
			  { base::StrID("builtin_lock_mutex"),
			    code::FuncSignature({}, { base::StrID("mutex") }) } },
			{ BuiltinFunctionID::UnlockMutex,
			  { base::StrID("builtin_unlock_mutex"),
			    code::FuncSignature({}, { base::StrID("mutex") }) } },
			{ BuiltinFunctionID::DestroyMutex,
			  { base::StrID("builtin_destroy_mutex"),
			    code::FuncSignature({}, { base::StrID("mutex") }) } },
			{ BuiltinFunctionID::CreateCV,
			  { base::StrID("builtin_create_cv"),
			    code::FuncSignature({ base::StrID("condition_variable") }, {}) } },
			{ BuiltinFunctionID::WaitCV,
			  { base::StrID("builtin_wait_cv"),
			    code::FuncSignature(
					{}, { base::StrID("condition_variable"), base::StrID("mutex") }
				) } },
			{ BuiltinFunctionID::NotifyCV,
			  { base::StrID("builtin_notify_cv"),
			    code::FuncSignature({}, { base::StrID("condition_variable") }) } },
			{ BuiltinFunctionID::NotifyAllCV,
			  { base::StrID("builtin_notify_all_cv"),
			    code::FuncSignature({}, { base::StrID("condition_variable") }) } },
			{
				BuiltinFunctionID::DestroyCV,
				{ base::StrID("builtin_destroy_cv"),
			      code::FuncSignature({}, { base::StrID("condition_variable") }) },
			},
		};

		return &map;
	}

	base::Optional<CRef<code::FuncSignature>> getBuiltinFunctionSignature(base::StrID name) {
		return getBuiltinFunctionID(name).map([](const auto& id) {
			return getBuiltinFunctionSignature(id);
		});
	}

	base::Optional<BuiltinFunctionID> getBuiltinFunctionID(base::StrID name) {
		// Lazy initialization of the "builtin name -> id" map.
		static const std::unordered_map<base::StrID, BuiltinFunctionID> builtin_function_indices
			= [] {
				  std::unordered_map<base::StrID, BuiltinFunctionID> indices;

				  for (const auto& [id, func]: *getBuiltinFunctions())
					  indices.emplace(func.name, id);
				  return indices;
			  }();

		auto it = builtin_function_indices.find(name);
		if (it != builtin_function_indices.end()) return it->second;
		return {};
	}

	bool isBuiltinFunction(base::StrID name) { return getBuiltinFunctionID(name).has_value(); }
}
