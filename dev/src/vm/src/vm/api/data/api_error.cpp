#include "api_error.hpp"

#include <cerrno>

namespace vm::api {
	std::string errorToString(const ApiError& api_error) {
		return nlohmann::to_string(nlohmann::json(api_error));
	}

	int errorToErrno(const ApiError& api_error) {
		using namespace std;

		struct Visitor final {
			int operator()(const ResumeError&) const noexcept { return EINVAL; }

			int operator()(const PauseError&) const noexcept { return EINVAL; }

			int operator()(const RunError&) const noexcept { return EINVAL; }

			int operator()(const JoinError&) const noexcept { return EINVAL; }

			int operator()(const AttachDetachError&) const noexcept { return EINVAL; }

			int operator()(const OtherError&) const noexcept { return EINVAL; }

			int operator()(const IOError&) const noexcept { return EIO; }

			int operator()(const LoadProgramError&) const noexcept { return EINVAL; }

			int operator()(const ProcessNotFound&) const noexcept { return ESRCH; }

			int operator()(const WrongResponse&) const noexcept { return EIO; }

			int operator()(const StateError&) const noexcept { return EINVAL; }

			int operator()(const Panicked&) const noexcept { return EINVAL; }

			int operator()(const NotImplementedError&) const noexcept { return ENOSYS; }

			int operator()(const UnsupportedOperation&) const noexcept { return ENOTSUP; }
		};

		return std::visit(Visitor{}, api_error);
	}
}
