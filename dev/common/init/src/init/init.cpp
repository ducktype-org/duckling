#include "init.hpp"
#include <base/ref.hpp>
#include <base/exceptions.hpp>
#include <iostream>

namespace init {
	namespace {
		/**
		 * Helper struct for storing module global state
		**/
		struct InitState final {
			std::vector<std::function<void()>> init_function_list;
			std::vector<std::function<void()>> deinit_function_list;
			bool was_init = false;
			bool was_deinit = false;

			~InitState() {
				if (not was_deinit or not was_init) {
					std::cerr << "ERROR: Init module was linked but never used!\n";
					std::terminate();
				}
			}
		};

		/**
		 * Wrapper around InitState object instance.
		 * This way it can be safely used before main is called,
		 * preventing static initialization order fiasco.
		 */
		Ref<InitState> getInitState() {
			static InitState deinit_static;
			return &deinit_static;
		}
	}

	void registerForInit(std::function<void()> function) {
		auto state = getInitState();
		CORE_ASSERT(not state->was_init, "Cannot register for init after init");
		state->init_function_list.push_back(std::move(function));
	}

	void registerForDeinit(std::function<void()> function) {
		auto state = getInitState();
		CORE_ASSERT(not state->was_deinit, "Cannot register for deinit after deinit");
		state->deinit_function_list.push_back(std::move(function));
	}

	InitObject::InitObject() {
		auto state = getInitState();
		CORE_ASSERT(not state->was_init, "InitObject can only be created once");
		state->was_init = true;

		for (auto& function : state->init_function_list) {
			function();
		}
	}

	InitObject::~InitObject() {
		auto state = getInitState();
		if (not state->was_deinit) {
			// We can't really throw here, since it is
			// a destructor, so we terminate instead.
			// Double creation sanity check in InitObject() should disallow this
			// to even happen, but better to be safe then sorry here.
			std::cerr << "ERROR: DeinitObject can't be destroyed twice\n";
			std::terminate();
		}
		state->was_deinit = true;

		for (auto& function : state->deinit_function_list) {
			function();
		}
	}
}
