#pragma once

#include "bfs_linearizer.hpp"
#include "coordinator.hpp"
#include "executor.hpp"
#include "history.hpp"

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <iostream>

namespace concurrent::tester {
	/**
	 * A tester for races in a concurrent data structure.
	 * It records operations performed concurrently on the tested implementation,
	 * and then performs an exhaustive search on a sequential implementation to see
	 * if the recorded history is linearizable (i.e. possible to reproduce with sequential
	 * operations).
	 *
	 * @tparam TestedInterface The interface that both implementations satisfy.
	 * @tparam TestedImplementation The implementation type of the tested class.
	 * @tparam SequentialImplementation The implementation type of the sequential class.
	 * It must be copy-constructible for the purpose of the linearizability search.
	 * @tparam PossibleResults The possible result types of the operations.
	 * Use std::monostate instead of void.
	 */
	template<
		class TestedInterface,
		std::derived_from<TestedInterface> TestedImplementation,
		std::derived_from<TestedInterface> SequentialImplementation,
		typename... PossibleResults>
	requires std::is_copy_constructible_v<SequentialImplementation> class RaceTester {
	public:
		using History_     = History<TestedInterface, PossibleResults...>;
		using Executor_    = Executor<TestedInterface, PossibleResults...>;
		using Coordinator_ = Coordinator<TestedInterface, PossibleResults...>;

		RaceTester(
			Ref<TestedImplementation>      tested_instance,
			CRef<SequentialImplementation> sequential_instance
		):
			  tested_instance(tested_instance),
			  sequential_instance(sequential_instance) {}

		/**
		 * Run multiple worker threads to record their race history.
		 *
		 * Each worker gets its ID and an executor. The worker should use the `Executor::execute()`
		 * method to perform operations on the tested instance, but providing function objects
		 * which act on a reference to the tested instance, as well as a human-readable description.
		 *
		 * @param worker_count The number of workers to run (they're indexed starting from 0).
		 * @param worker The implementation of the workers.
		 */
		void run(const u32 worker_count, std::function<void(u32, Executor_)> worker) {
			history = makeBox<History_>(worker_count);
			Coordinator_ coordinator(tested_instance, history.refMut().toOpt().value());
			coordinator.runWorkers(worker_count, worker);
		}

		CRef<History_> getHistory() const { return history.ref().toOpt().value(); }

		/**
		 * Check whether the history recorded by `run` is linearizable. Print it if it isn't.
		 * @return True if and only if the recorded history was linearizable.
		 */
		bool check() {
			BFSLinearizer<TestedInterface, SequentialImplementation, PossibleResults...> linearizer{
				history.ref().toOpt().value(), *sequential_instance
			};
			base::Optional<usize> linearization_result = linearizer.linearize();
			if (linearization_result.has_value()) {
				std::cerr << history->toString(linearization_result.value()) << std::endl;
				return false;
			}
			return true;
		}

		/**
		 * Record the race of multiple worker threads and check whether it is linearizable.
		 * @param worker_count The number of workers to run (they're indexed starting from 0).
		 * @param worker The implementation of the workers.
		 * @return True if and only if the recorded history was linearizable.
		 */
		bool runAndCheck(const u32 worker_count, std::function<void(u32, Executor_)> worker) {
			run(worker_count, worker);
			return check();
		}

	private:
		/**
		 * @brief A reference to the tested instance.
		 */
		Ref<TestedInterface> tested_instance;

		/**
		 * @brief A reference to the sequential implementation.
		 */
		CRef<SequentialImplementation> sequential_instance;

		/**
		 * @brief The history of operations.
		 */
		MBox<History_> history{};
	};
}
