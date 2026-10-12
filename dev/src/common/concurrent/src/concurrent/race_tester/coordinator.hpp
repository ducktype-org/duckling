// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "executor.hpp"
#include "history.hpp"

#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <barrier>
#include <thread>

namespace concurrent::tester {
	/**
	 * Executes and logs operations performed on a concurrent data structure and logs a history.
	 * @tparam TestedInterface The interface of the tested concurrent data structure.
	 * @tparam PossibleResults The possible result types of the operations.
	 * Use std::monostate instead of void.
	 */
	template<class TestedInterface, typename... PossibleResults>
	class Coordinator {
	public:
		using Executor_       = Executor<TestedInterface, PossibleResults...>;
		using History_        = History<TestedInterface, PossibleResults...>;
		using ResultsVariant_ = typename History_::ResultsVariant_;

		/**
		 * @brief Run multiple worker threads to perform operations on the tested instance.
		 *
		 * The workers get the thread ID and a reference to an executor.
		 * They should use the Executor::execute() method to act on the tested instance.
		 *
		 * A worker may use its ID to direct its actions, for example when testing
		 * interactions between workers in different "roles".
		 *
		 * An example of a valid worker is one which performs 10 000 operations in a loop,
		 * each time randomly choosing to add an element to a collection, or to remove an element.
		 * The operations are executed using Executor::execute(), by passing a function object that
		 * performs the appropriate operation on a reference to the collection, and a string which
		 * represents the operation in a way that is understandable to a human.
		 *
		 * @param worker_count How many workers are to be spawned.
		 * @param worker The implementation of a worker.
		 */
		void runWorkers(const u32 worker_count, std::function<void(u32, Executor_)> worker) {
			std::barrier              start_barrier(worker_count);
			std::vector<std::jthread> threads;
			threads.reserve(worker_count);

			for (u32 i = 0; i < worker_count; ++i) {
				Executor_ executor(tested_instance, i, history);
				threads.emplace_back([i, &worker, executor, &start_barrier] {
					start_barrier.arrive_and_wait();
					worker(i, executor);
				});
			}
		}

		Coordinator(Ref<TestedInterface> tested_instance, Ref<History_> history):
			  tested_instance(tested_instance),
			  history(history) {}

	private:
		/**
		 * @brief A reference to the tested instance.
		 */
		Ref<TestedInterface> tested_instance;

		/**
		 * @brief A reference to the history in which executed operations will be recorded.
		 */
		Ref<History_> history;
	};
}
