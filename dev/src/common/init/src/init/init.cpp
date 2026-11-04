// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "init.hpp"

#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <cstdlib>
#include <iostream>

namespace init {
	namespace {

		/**
		 * This is essentially a duplication of
		 * InitState::was_init, but defined in a way
		 * that should make it safe to call after main,
		 * and more precisely, during static deinitialization
		 * of init_verifier static object defined below.
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
		 * Helper function to suppress undue termination on std::exit
		 * since automatic storage duration objects' dtors are not called.
		 * Ensured that the function is only registered on InitObject construction.
		 */
		void initStateAtexitHandler() {
			if (not getInitState()->was_deinit) {
				std::cerr << "WARNING: InitObject was used but its dtor was not called. "
							 "This can happen for example when std::exit was used or when "
							 "exception was not caught, "
							 "and should be treated as a bug in the proper compiler executable.";
			}
			getInitState()->was_deinit = true;
			if (not getInitState()->was_init) {
				// this should only be called if InitObject was used.
				std::terminate();
			}
		}

		/**
		 * Helper struct to ensure that InitObject
		 * was created and used correctly.
		 */
		struct InitVerifier final {
			/**
			 * We have it here,
			 * just to make init_verifier static object
			 * odr-used so no unexpected things happen
			 * with its initialization/deinitization.
			 */
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
		// so it won't be ignored for some weird reason:
		init_verifier.dummy = 1;

		auto state = getInitState();
		CORE_ASSERT(not state->was_init, "InitObject can only be created once");
		state->was_init = true;

		auto handler_fail = std::atexit(initStateAtexitHandler);
		CORE_ASSERT(not handler_fail, "Cannot register atexit handler");

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

	bool wasInitObject() { return getInitState()->was_init; }
}
