#pragma once

#include <vm/api/vm.hpp>

/**
 * @file vm_debug.hpp
 * @brief Command line interface for the VM debugger.
 */


class DuckVMDebug {
public:
	using OwnedArgumentList = std::vector<Box<vm::VmValue>>;

	static DuckVMDebug get(const fs::File& filepath, const std::vector<std::string>& args = {});

	DuckVMDebug(DuckVMDebug&&)                 = delete;
	DuckVMDebug& operator=(DuckVMDebug&&)      = delete;
	DuckVMDebug(const DuckVMDebug&)            = delete;
	DuckVMDebug& operator=(const DuckVMDebug&) = delete;

	void run() const;

private:
	struct CallInfo {
		std::string       func_name;
		OwnedArgumentList func_args;
	};

	vm::PID pid{};
	u64     step_counter = 0;

	std::vector<std::string> debug_args;

	DuckVMDebug();
	DuckVMDebug(const fs::File& filepath, const std::vector<std::string>& args = {});

	void runVm() const;
	void runFun(const std::string& string) const;
	void getExitValue() const;
	void getStatus() const;
	void step() const;
	void resume() const;
	void pause() const;
};

class DuckVMDebugException: public base::Exception {
	std::string message;

public:
	DuckVMDebugException(std::string message): base::Exception(), message(std::move(message)) {}

	[[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
};

#define DEFINE_DEBUG_EXCEPTION(err, msg)                     \
struct err: public DuckVMDebugException {                \
constexpr static std::string_view ERR_MSG = msg;    \
err(): DuckVMDebugException(std::string(ERR_MSG)) {} \
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