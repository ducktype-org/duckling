#pragma once

#include <vm/api/vm.hpp>

/**
 * @file vm_debug_core.hpp
 * @brief Core for the VM debugger.
 */


class DuckVMDebugCore {
public:
	using OwnedArgumentList = std::vector<Box<vm::VmValue>>;

	static DuckVMDebugCore get(const fs::File& filepath, const std::vector<std::string>& args = {});

	DuckVMDebugCore(DuckVMDebugCore&&)                 = delete;
	DuckVMDebugCore& operator=(DuckVMDebugCore&&)      = delete;
	DuckVMDebugCore(const DuckVMDebugCore&)            = delete;
	DuckVMDebugCore& operator=(const DuckVMDebugCore&) = delete;

	DuckVMDebugCore();
	DuckVMDebugCore(const fs::File& filepath, const std::vector<std::string>& args = {});

	[[nodiscard]] int getEventPipeReadFD() const { return event_pipe[0]; }

	void runVm();
	void runFun(const std::string& string);
	void getStatus() const;
	void step() const;
	void resume() const;
	void pause() const;
	void stop() const;
	void getCurrentPosition() const;

private:
	struct CallInfo {
		std::string       func_name;
		OwnedArgumentList func_args;
	};
	int event_pipe[2]; // event_pipe[0] = read end, event_pipe[1] = write end

	vm::PID pid{};
	u64     step_counter = 0;

	std::vector<std::string> debug_args;
	OwnedArgumentList arguments;
};

class DuckVMDebugCoreException: public base::Exception {
	std::string message;

public:
	DuckVMDebugCoreException(std::string message): base::Exception(), message(std::move(message)) {}

	[[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
};

#define DEFINE_DEBUG_EXCEPTION(err, msg)                     \
struct err: public DuckVMDebugCoreException {                \
constexpr static std::string_view ERR_MSG = msg;    \
err(): DuckVMDebugCoreException(std::string(ERR_MSG)) {} \
}

DEFINE_DEBUG_EXCEPTION(BeRDFailedToSpawnProcessException, "Failed to spawn the VM process for BeRD.");
DEFINE_DEBUG_EXCEPTION(
	BeRDFailedToAttachStreamsException, "Failed to attach streams to the VM process for BeRD."
);
DEFINE_DEBUG_EXCEPTION(BeRDFailedToJoinProcessException, "Failed to join the VM process for BeRD.");
DEFINE_DEBUG_EXCEPTION(BeRDFailedToLoadFile, "Failed to load files to the VM process for BeRD.");
DEFINE_DEBUG_EXCEPTION(BeRDFailedToPauseVM, "Failed to pause the VM process for BeRD.");
DEFINE_DEBUG_EXCEPTION(BeRDFailedToResumeVM, "Failed to resume the VM process for BeRD.");
DEFINE_DEBUG_EXCEPTION(BeRDFailedToMakeStep, "Failed to make one step in the VM process for BeRD.");
DEFINE_DEBUG_EXCEPTION(BeRDFailedToRunCodeException, "Failed to run code in BeRD.");
DEFINE_DEBUG_EXCEPTION(BeRDWrongTypeException, "BeRD encountered a different type than 'i64'");
DEFINE_DEBUG_EXCEPTION(BeRDFailedToCreateAVmValue, "Failed to create a VmValue for arguments.");
DEFINE_DEBUG_EXCEPTION(BeRDEmptyExitCodeException, "Exit code is empty, cannot continue.");