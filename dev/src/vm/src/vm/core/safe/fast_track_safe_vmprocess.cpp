#include "fast_track_safe_vmprocess.hpp"

#include <vm/core/safe/low_program/low_program.hpp>

#include <thread>

namespace vm {

	void FastTrackSafeVMProcess::onBeforeThreadSpawn(SafeVMThread& base_child) {
		auto& child = static_cast<FastTrackSafeVMThread&>(base_child);

		FastTrackSafeVMThread* parent_thread = nullptr;
		auto                   current_id    = std::this_thread::get_id();
		for (auto& t: ft_vm_threads) {
			if (t.hasActiveThread() && t.getNativeThreadId() == current_id) {
				parent_thread = &t;
				break;
			}
		}

		const VectorClock& parent_vc = parent_thread
		                                 ? parent_thread->getFTData().getVC()
		                                 : static_cast<FastTrackSafeVMThread&>(doGetMainThread())
		                                       .getFTData()
		                                       .getVC();

		child.getFTData().forkVC(parent_vc, child.getThreadID());
	}

	FastTrackSafeVMProcess::FastTrackSafeVMProcess(
		PID                       my_pid,
		const api::ProcessSettings& settings
	):
		  SafeVMProcess(my_pid, settings) {
		ft_vm_threads.add(*this);
	}

	// ---- Virtual thread-pool overrides ----

	SafeVMThread& FastTrackSafeVMProcess::doGetMainThread() {
		return *ft_vm_threads.get(api::ThreadID{ 0 });
	}

	SafeVMThread& FastTrackSafeVMProcess::doGetOrCreateEmptyThread() {
		for (auto& thread: ft_vm_threads) {
			if (!api::isExecuting(thread.getStatus()) && !thread.hasActiveThread()) return thread;
		}
		return *ft_vm_threads.get(ft_vm_threads.add(*this));
	}

	base::Optional<Ref<SafeVMThread>> FastTrackSafeVMProcess::doGetThreadByID(
		api::ThreadID thread_id
	) {
		if_opt_some(ft_vm_threads.maybeGet(thread_id), thread) return thread;
		return std::nullopt;
	}

	void FastTrackSafeVMProcess::forEachThread(const std::function<void(SafeVMThread&)>& fn) {
		for (auto& thread: ft_vm_threads) fn(thread);
	}

	// ---- Global data / shadow buffer initialisation ----

	void FastTrackSafeVMProcess::updateGlobalDataMemory(CRef<low::ILowVMProgram> program) {
		SafeVMProcess::updateGlobalDataMemory(program);

		ft_globals.initialize(program);

		for (auto& thread: ft_vm_threads) {
			thread.updateFTGlobalPointers(ft_globals.globalShadowDataBase());
		}
	}
}
