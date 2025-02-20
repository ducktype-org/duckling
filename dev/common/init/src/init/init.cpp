#include "init.hpp"
#include <base/ref.hpp>
#include <base/exceptions.hpp>
#include <iostream>

namespace init {
	namespace {

		/**
		 * This is essentially a duplication of
		 * InitState::was_init, but defined in a way
		 * that should make it safe to call after main,
		 * and more precisely, during static initialization
		 * of init_verifier static object defined bellow.
		 *
		 * It is used for sanity check that init was used
		 * when it was linked.
		 */
		constinit bool init_was_created = false;

		/**
		 * Helper struct for storing module global state
		 **/
		struct InitState final {
			std::vector<std::function<void()>> init_function_list;
			std::vector<std::function<void()>> deinit_function_list;
			bool                               was_init   = false;
			bool                               was_deinit = false;

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

		/**
		 * Helper struct to ensure that InitObject
		 * was created and used correctly.
		 */
		struct InitVerifier final {
			int dummy = 0;

			~InitVerifier() {
				if (not init_was_created) {
					std::cerr << "ERROR: InitObject was never created!\n";
					std::terminate();
				}
			}
		};

		constinit InitVerifier init_verifier;
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
		init_was_created = true;

		// we do it just so init_verifier is used
		// so it wont be ignored for some weird reason:
		init_verifier.dummy = 1;

		auto state = getInitState();
		CORE_ASSERT(not state->was_init, "InitObject can only be created once");
		state->was_init = true;

		for (auto& function: state->init_function_list) function();
	}

	InitObject::~InitObject() {
		auto state = getInitState();
		if (state->was_deinit) {
			// We can't really throw here, since it is
			// a destructor, so we terminate instead.
			// Double creation sanity check in InitObject() should disallow this
			// to even happen, but better to be safe then sorry here.
			std::cerr << "ERROR: InitObject can't be destroyed twice\n";
			std::terminate();
		}
		state->was_deinit = true;

		for (auto& function: state->deinit_function_list) function();
	}
}
