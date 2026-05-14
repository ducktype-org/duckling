#include "fast_vmthread.hpp"

#include "fast_vmprocess.hpp"

#include "vm/core/thread/ivmthread.hpp"

using namespace vm;

vm::fast::FastVMThread::FastVMThread(api::ThreadID thread_id, FastVMProcess& process):
	  IVMThread(thread_id, process),
	  fast_process(process) {

      }

std::expected<api::Response, api::ApiError> vm::fast::FastVMThread::getCurrentPosition() {
	return std::unexpected(
		api::ApiError{
			api::NotImplementedError{ "Method `getCurrentPosition` is not implemented." } }
	);
}

[[nodiscard]] u64 vm::fast::FastVMThread::getNumberOfCurrentStackFrames() const {
	throw vm::VMNotImplemented("Method `getNumberOfCurrentStackFrames` is not implemented.");
}

void vm::fast::FastVMThread::run(const std::string&, const RunArguments&) {
	throw vm::VMNotImplemented("Method `run` is not implemented.");
}

void vm::fast::FastVMThread::executeOneStep() {
	throw vm::VMNotImplemented("Method `executeOneStep` is not implemented.");
}

void vm::fast::FastVMThread::execGlobalDestructors() {
	// TODO: Implement this pure virtual method.
	throw vm::VMNotImplemented("Method `execGlobalDestructors` is not implemented.");
}
