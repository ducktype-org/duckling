#include "vm/utils/vm_not_implemented.hpp"
#include <vm/core/thread/ivmthread.hpp>

namespace vm::fast {
    class IVMThread: public vm::IVMThread {
	public:
		std::expected<api::Response, api::ApiError> getCurrentPosition() override {
            return std::unexpected(api::ApiError{ api::NotImplementedError{ "Method `getCurrentPosition` is not implemented." } });
		}

		[[nodiscard]] u64 getNumberOfCurrentStackFrames() const override {
            throw vm::VMNotImplemented("Method `getNumberOfCurrentStackFrames` is not implemented.");
		}


	protected:
		void run(const std::string& func_name, const RunArguments& run_arguments) override {
			// TODO: Implement this pure virtual method.
			static_assert(false, "Method `run` is not implemented.");
		}

		void executeOneStep() override {
			// TODO: Implement this pure virtual method.
			static_assert(false, "Method `executeOneStep` is not implemented.");
		}

		void execGlobalDestructors() override {
			// TODO: Implement this pure virtual method.
			static_assert(false, "Method `execGlobalDestructors` is not implemented.");
		}
	};
}
