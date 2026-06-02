#pragma once

#include <vm/core/process/concurrency/fast_track/fast_track_globals.hpp>
#include <vm/core/safe/fast_track_safe_vmthread.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>

#include <base/collections/object_pool.hpp>

namespace vm {

	/**
	 * @brief Fast Track-enabled process. Extends SafeVMProcess with global shadow buffers,
	 * shadow memory allocators, and a pool of FastTrackSafeVMThread instances.
	 *
	 * Instantiated by the supervisor when ProcessSettings::enable_fast_track is true.
	 */
	class FastTrackSafeVMProcess final : public SafeVMProcess {
		FastTrackGlobals ft_globals;
		base::StableObjectPool<FastTrackSafeVMThread, api::ThreadID, false, true> ft_vm_threads;

	protected:
		SafeVMThread&                     doGetMainThread() override;
		SafeVMThread&                     doGetOrCreateEmptyThread() override;
		base::Optional<Ref<SafeVMThread>> doGetThreadByID(api::ThreadID thread_id) override;
		void forEachThread(const std::function<void(SafeVMThread&)>& fn) override;
		void onBeforeThreadSpawn(SafeVMThread& child) override;

	public:
		FastTrackSafeVMProcess(PID my_pid, const api::ProcessSettings& settings);

		FastTrackGlobals& getFTGlobals() { return ft_globals; }

		void updateGlobalDataMemory(CRef<low::ILowVMProgram> program) override;

		friend class FastTrackSafeVMThread;
	};
}
