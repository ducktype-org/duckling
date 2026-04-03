#include "builtin_functions.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/concurrency/synchronization_primitives.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/process/safe_vmprocess.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/core/thread/vmvalue.hpp>

namespace vm::builtins {

	namespace {
		template<class Ret, class... FunArgs, std::size_t... Is>
		base::Optional<Box<VmValue>> callUnpackArgsImpl(
			Ret (*function)(VMThread&, FunArgs...),
			const std::vector<TypeCRef>&     vm_return_types,
			VMProcess&                       process,
			VMThread&                        thread,
			const std::vector<Box<VmValue>>& args,
			std::index_sequence<Is...>
		) {
			if constexpr (std::is_void_v<Ret>) {
				function(thread, args[Is]->template readBytes<FunArgs>()...);
				return {};
			} else {
				CORE_ASSERT(
					vm_return_types.size() == 1,
					"Builtin Function with multiple more than 1 return value"
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
			Ret (*function)(VMThread&, FunArgs...),
			const std::vector<TypeCRef>&     vm_return_types,
			VMProcess&                       process,
			VMThread&                        thread,
			const std::vector<Box<VmValue>>& args
		) {
			CORE_ASSERT(
				sizeof...(FunArgs) == args.size(),
				"Wrong number of arguments passed to the builtin function"
			);
			return callUnpackArgsImpl(
				function, vm_return_types, process, thread, args, std::index_sequence_for<FunArgs...>{}
			);
		}
	}

	// ============================== BUILTIN IMPLEMENTATIONS ==============================

	i64 FunctionHandlers::builtinInputI64(VMThread& thread) {
		thread.setProcessStatus(api::WaitingForInput{});
		auto return_value = thread.process.getIO().getInput<i64>(thread);
		thread.setProcessStatus(api::Running{});
		return return_value;
	}

	i64 FunctionHandlers::builtinOutputI64(VMThread& thread, i64 arg) {
		const std::string output = std::to_string(arg) + "\n";
		thread.process.getIO().writeOutput(output);

		return base::safeIntConv<i64>(output.size());
	}

	void FunctionHandlers::builtinOutputString(VMThread& thread, Pointer ptr) {
		auto block      = ptr.getBlock();
		auto block_id   = thread.process_memory.requestBlockID(block);
		auto block_data = thread.process_memory.requestBlockData(block_id);
		auto str_data   = block_data.stdString();
		thread.process.getIO().writeOutput(str_data.substr(0, str_data.size() - 1) + "\n");
	}

	i64 FunctionHandlers::builtinStoi(VMThread& thread, Pointer ptr) {
		auto block      = ptr.getBlock();
		auto block_id   = thread.process_memory.requestBlockID(block);
		auto block_data = thread.process_memory.requestBlockData(block_id);

		auto str_data = block_data.stdString();
		return std::stoll(str_data);
	}

	i64 FunctionHandlers::builtinStartThread(VMThread& thread) {
		return i64{ vm::api::runFunction(thread.process.getPID(), thread.getThreadCtx()).value() };
	}

	void FunctionHandlers::builtinJoinThread(VMThread& thread, i64 thread_id) {
		thread.releaseGil();
		vm::api::join(thread.process.getPID(), api::ThreadID{ thread_id });
		thread.acquireGil();
	}

	u64 FunctionHandlers::builtinCreateMutex(VMThread& thread) {
		return thread.process.getSynchronizationPrimitives().addMutex();
	}

	void FunctionHandlers::builtinLockMutex(VMThread& thread, u64 mutex_id) {
		auto mutex = thread.process.getSynchronizationPrimitives().getMutex(mutex_id);

		if (!mutex->try_lock()) {
			thread.releaseGil();
			mutex->lock();
			thread.acquireGil();
		}
	}

	void FunctionHandlers::builtinUnlockMutex(VMThread& thread, u64 mutex_id) {
		auto mutex = thread.process.getSynchronizationPrimitives().getMutex(mutex_id);
		mutex->unlock();
	}

	void FunctionHandlers::builtinDestroyMutex(VMThread& thread, u64 mutex_id) {
		thread.process.getSynchronizationPrimitives().removeMutex(mutex_id);
	}

	u64 FunctionHandlers::builtinCreateCV(VMThread& thread) {
		return thread.process.getSynchronizationPrimitives().addCV();
	}

	void FunctionHandlers::builtinWaitCV(VMThread& thread, u64 cv_id, u64 mutex_id) {
		auto cv    = thread.process.getSynchronizationPrimitives().getCV(cv_id);
		auto mutex = thread.process.getSynchronizationPrimitives().getMutex(mutex_id);

		thread.releaseGil();
		try {
			cv->wait(*mutex);
		} catch (const vm::exceptions::VMRuntimeException&) {
			// Ensure the GIL is held again before propagating VM runtime exceptions.
			thread.acquireGil();
			throw;
		} catch (const std::exception& e) {
			// Reacquire GIL and wrap standard exceptions so the VM can report ExecutionPanicked.
			thread.acquireGil();
			std::string msg = "builtinWaitCV failed during condition variable wait: ";
			msg += e.what();
			throw vm::exceptions::VMRuntimeException(std::move(msg));
		} catch (...) {
			// Reacquire GIL and convert unknown exceptions into a VMRuntimeException.
			thread.acquireGil();
			throw vm::exceptions::VMRuntimeException(
				"builtinWaitCV failed during condition variable wait with an unknown exception"
			);
		}
		thread.acquireGil();
	}

	void FunctionHandlers::builtinNotifyCV(VMThread& thread, u64 cv_id) {
		auto cv = thread.process.getSynchronizationPrimitives().getCV(cv_id);
		cv->notifyOne();
	}

	void FunctionHandlers::builtinNotifyAllCV(VMThread& thread, u64 cv_id) {
		auto cv = thread.process.getSynchronizationPrimitives().getCV(cv_id);
		cv->notifyAll();
	}

	void FunctionHandlers::builtinDestroyCV(VMThread& thread, u64 cv_id) {
		thread.process.getSynchronizationPrimitives().removeCV(cv_id);
	}

	base::Optional<Box<VmValue>> callBuiltinFunction(
		BuiltinFunctionID                id,
		std::vector<TypeCRef>            result_types,
		VMProcess&                       process,
		VMThread&                        thread,
		const std::vector<Box<VmValue>>& arguments
	) {
		switch (id) {
#define CASE_FUNC(ID_NAME)                                                              \
	case BuiltinFunctionID::ID_NAME: {                                                  \
		return callUnpackArgs(                                                          \
			FunctionHandlers::builtin##ID_NAME, result_types, process, thread, arguments \
		);                                                                              \
	}

			FOR_EACH(
				CASE_FUNC,
				InputI64,
				OutputI64,
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
				DestroyCV
			)


		default:
			CORE_PANIC("Invalid builtin function ID");
		}
	}

	auto getBuiltinFunctions()
		-> CRef<std::unordered_map<BuiltinFunctionID, std::pair<base::StrID, code::FuncSignature>>> {
		static const std::unordered_map<BuiltinFunctionID, std::pair<base::StrID, code::FuncSignature>>
			map{
				{
					BuiltinFunctionID::InputI64,
					{ base::StrID("builtin_input_i64"),
			          code::FuncSignature({ base::StrID("i64") }, {}) },
				},
				{
					BuiltinFunctionID::OutputI64,
					{ base::StrID("builtin_output_i64"),
			          code::FuncSignature({ base::StrID("i64") }, { base::StrID("i64") }) },
				},
				{
					BuiltinFunctionID::OutputString,
					{ base::StrID("builtin_strOutput_lptr"),
			          code::FuncSignature({}, { base::StrID("ptr_string") }) },
				},
				{
					BuiltinFunctionID::Stoi,
					{
						base::StrID("builtin_stoi_lptr"),
						code::FuncSignature({ base::StrID("i64") }, { base::StrID("ptr_string") }),
					},
				},
				{
					BuiltinFunctionID::StartThread,
					{ base::StrID("builtin_start_thread"),
			          code::FuncSignature({ base::StrID("i64") }, {}) },
				},
				{
					BuiltinFunctionID::JoinThread,
					{ base::StrID("builtin_join_thread"),
			          code::FuncSignature({}, { base::StrID("i64") }) },
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
				{ BuiltinFunctionID::DestroyCV,
			      { base::StrID("builtin_destroy_cv"),
			        code::FuncSignature({}, { base::StrID("condition_variable") }) } },
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

				  for (const auto& [id, func_pair]: *getBuiltinFunctions())
					  indices.emplace(func_pair.first, id);
				  return indices;
			  }();

		auto it = builtin_function_indices.find(name);
		if (it != builtin_function_indices.end()) return it->second;
		return {};
	}

	bool isBuiltinFunction(base::StrID name) { return getBuiltinFunctionID(name).has_value(); }
}
