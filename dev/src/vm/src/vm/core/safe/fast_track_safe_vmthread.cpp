#include "fast_track_safe_vmthread.hpp"
#include "fast_track_safe_vmprocess.hpp"

namespace vm {

	FastTrackSafeVMThread::FastTrackSafeVMThread(
		api::ThreadID  thread_id,
		SafeVMProcess& process
	):
		  SafeVMThread(thread_id, process) {
		auto& ft_process = static_cast<FastTrackSafeVMProcess&>(process);
		auto& globals    = ft_process.getFTGlobals();
		ft_data.init(
			ft_stack,
			thread_id,
			globals.globalShadowDataBase()
		);
	}

	void FastTrackSafeVMThread::onBeforeExecute() {
		auto& rt = ft_data.ft_runtime;
		auto* sf = rt.shadow_frame_stack_current;
		sf->local_shadow_data_stack = rt.shadow_data_stack_base;
		sf->local_shadow_data_head  = 0;
	}

	IMemory<ShadowEntry>& FastTrackSafeVMThread::getShadowDataMemory() {
		return static_cast<FastTrackSafeVMProcess&>(safe_process).getFTGlobals().getShadowDataMemory();
	}
}
