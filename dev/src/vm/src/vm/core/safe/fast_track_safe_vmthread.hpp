#pragma once

#include <vm/core/process/concurrency/fast_track/fast_track_thread_data.hpp>
#include <vm/core/safe/memory/fast_track_thread_stack.hpp>
#include <vm/core/safe/safe_vmthread.hpp>

namespace vm {
	namespace builtins {
		class FunctionHandlers;
	}
	class FastTrackSafeVMProcess;

	/**
	 * @brief Fast Track-enabled thread. Extends SafeVMThread with shadow stacks and a
	 * vector clock. Only instantiated when ProcessSettings::enable_fast_track is true.
	 */
	class FastTrackSafeVMThread final : public SafeVMThread {
		FastTrackThreadStack ft_stack;
		FastTrackThreadData  ft_data;

	public:
		FastTrackSafeVMThread(api::ThreadID thread_id, SafeVMProcess& process);

		FastTrackThreadData& getFTData() { return ft_data; }
		const FastTrackThreadData& getFTData() const { return ft_data; }

		IMemory<ShadowEntry>&   getShadowDataMemory();

		void updateFTGlobalPointers(ShadowEntry* global_data) {
			ft_data.updateGlobalPointers(global_data);
		}

	protected:
		void onBeforeExecute() override;

	public:
		friend class OpFuns;
		friend class builtins::FunctionHandlers;
	};
}
