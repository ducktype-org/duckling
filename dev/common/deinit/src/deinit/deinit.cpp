#include "deinit.hpp"
#include <base/ref.hpp>

namespace deinit {
	namespace {
		struct DeinitHelper final {
			std::vector<std::function<void()>> function_list;

			~DeinitHelper() {
				for (auto& function: function_list) function();
			}
		};

		/**
		 * Wrapper around DeinitHelper object instance.
		 * This way it can be safely used before main is called,
		 * preventing static initialization order fiasco.
		 */
		Ref<DeinitHelper> getDeinitHelper() {
			/**
			 * @brief Static instance of DeinitHelper.
			 * When program terminates, its destructor will be called
			 * and all registered functions will be called with it.
			 */
			static DeinitHelper deinit_static;
			return &deinit_static;
		}
	}

	void registerForDeinit(std::function<void()> function) {
		getDeinitHelper()->function_list.push_back(std::move(function));
	}
}
