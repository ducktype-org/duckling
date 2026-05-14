#pragma once

#include <vm/utils/vm_not_implemented.hpp>
#include <vm/core/thread/ivmthread.hpp>

namespace vm::fast {
	class FastVMProcess;

    class FastVMThread: public vm::IVMThread {
	public:
		FastVMThread(api::ThreadID thread_id, FastVMProcess& process);

		std::expected<api::Response, api::ApiError> getCurrentPosition() override;

		[[nodiscard]] u64 getNumberOfCurrentStackFrames() const override;


	protected:
		void run(const std::string& func_name, const RunArguments& run_arguments) override;

		void executeOneStep() override;

		void execGlobalDestructors() override;

	private:
		FastVMProcess& fast_process;
	};
}
