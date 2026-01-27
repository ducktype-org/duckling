#pragma once

#include "executor.hpp"
#include "history.hpp"

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <mutex>
#include <thread>

namespace concurrent::tester {
	/**
	 * Executes and logs operations performed on a concurrent data structure and logs a history.
	 * @tparam TestedInterface The interface of the tested concurrent data structure.
	 * @tparam PossibleResults The possible result types of the operations.
	 * Use std::monostate instead of void.
	 */
	template<class TestedInterface, typename... PossibleResults>
	requires base::strConcatable<PossibleResults...> class Coordinator {
	public:
		using _Executor       = Executor<TestedInterface, PossibleResults...>;
		using _History        = History<TestedInterface, PossibleResults...>;
		using _ResultsVariant = typename _History::_ResultsVariant;

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
		void runWorkers(const u32 worker_count, std::function<void(u32, _Executor)> worker) {
			std::vector<std::jthread> threads;
			threads.reserve(worker_count);

			for (u32 i = 0; i < worker_count; ++i) {
				_Executor executor(tested_instance, i, history);
				threads.emplace_back([i, &worker, executor] { worker(i, executor); });
			}
		}

		Coordinator(Ref<TestedInterface> tested_instance, Ref<_History> history):
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
		Ref<_History> history;
	};
}
