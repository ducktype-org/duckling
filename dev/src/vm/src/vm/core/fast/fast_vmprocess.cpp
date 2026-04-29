#include "fast_vmprocess.hpp"

#include <vm/core/process/vmprocess.hpp>
#include <vm/utils/vm_not_implemented.hpp>

namespace vm::fast {
	FastVMProcess::FastVMProcess(PID pid): IVMProcess(pid) {}

	Ref<VmValue> FastVMProcess::createVmValue([[maybe_unused]] TypeCRef type) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `createVmValue` is not implemented.");
	}

	Ref<VmValue> FastVMProcess::createVmValue(
		[[maybe_unused]] TypeCRef type, [[maybe_unused]] Pointer src
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `createVmValue` is not implemented.");
	}

	Box<VmValue> FastVMProcess::createOwnedVmValue([[maybe_unused]] TypeCRef type) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `createOwnedVmValue` is not implemented.");
	}

	Box<VmValue> FastVMProcess::createOwnedVmValue(
		[[maybe_unused]] TypeCRef type, [[maybe_unused]] Pointer src
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `createOwnedVmValue` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::doRequest(
		const api::RequestVariant& request
	) {
		// So long api features are not implemented, this method will catch NotImplemented
		// exceptions thrown by them and return them as ApiErrors.
		try {
			return IVMProcess::doRequest(request);
		} catch (const VMNotImplemented& e) {
			return std::unexpected(api::ApiError{ api::NotImplementedError{ e.what() } });
		};
	}

	std::expected<api::Response, api::LoadProgramError> FastVMProcess::loadProgram(
		[[maybe_unused]] const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `loadProgram` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::runFunction(
		[[maybe_unused]] const std::string&  func_name,
		[[maybe_unused]] const RunArguments& run_arguments
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `runFunction` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::runFunctionAwait(
		[[maybe_unused]] const std::string&  func_name,
		[[maybe_unused]] const RunArguments& run_arguments
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `runFunctionAwait` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::join(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `join` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::stop() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `stop` is not implemented.");
	}

	std::expected<api::Response, api::StateError> FastVMProcess::getExitCode() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getExitCode` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::deinitAndValidate() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `deinitAndValidate` is not implemented.");
	}

	base::Optional<api::ApiError> FastVMProcess::pauseVMThread(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `pauseVMThread` is not implemented.");
	}

	base::Optional<api::ApiError> FastVMProcess::resumeVMThread(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `resumeVMThread` is not implemented.");
	}

	base::Optional<api::ApiError> FastVMProcess::stepVMThread([[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `stepVMThread` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getVMThreadCurrentPosition(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getVMThreadCurrentPosition` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getNumberOfCurrentStackFrames(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getNumberOfCurrentStackFrames` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getStackFrameData(
		[[maybe_unused]] api::ThreadID thread_id, [[maybe_unused]] u64 frame_index
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getStackFrameData` is not implemented.");
	}

	void FastVMProcess::notifyPausedVMThread([[maybe_unused]] api::ThreadID thread_id) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `notifyPausedVMThread` is not implemented.");
	}

	void FastVMProcess::waitForBreakpoint() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `waitForBreakpoint` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getTypeMetadata(
		[[maybe_unused]] const std::string& type_name
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getTypeMetadata` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getVMValueForType(
		[[maybe_unused]] const std::string& type_name
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getVMValueForType` is not implemented.");
	}

	std::vector<api::ThreadID> FastVMProcess::getAllThreadIDs() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getAllThreadIDs` is not implemented.");
	}

	api::ThreadID FastVMProcess::getMainThreadID() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getMainThreadID` is not implemented.");
	}
}
