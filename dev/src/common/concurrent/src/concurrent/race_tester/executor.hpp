#pragma once

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
	requires base::strConcatable<PossibleResults...> class Executor {
	public:
		using _History        = History<TestedInterface, PossibleResults...>;
		using _ResultsVariant = typename _History::_ResultsVariant;

		/**
		 * @brief Execute an operation on the tested instance; log the call and return events.
		 *
		 * @param description A human-readable description of the operation.
		 * @param operation A function object representing the operation to be executed.
		 */
		void execute(
			const std::string&                                   description,
			std::function<_ResultsVariant(Ref<TestedInterface>)> operation
		) {
			history->push_back(typename _History::Record{
				thread_id,
				typename _History::Call{
					.operation   = operation,
					.description = description,
				},
			});

			auto result = operation(tested_instance);

			history->push_back(typename _History::Record{
				thread_id,
				typename _History::Return{ std::move(result) },
			});
		}

		Executor(Ref<TestedInterface> tested_instance, const u32 thread_id, Ref<_History> history):
			  tested_instance(tested_instance),
			  thread_id(thread_id),
			  history(history) {}

	private:
		/**
		 * @brief A reference to the tested instance.
		 */
		Ref<TestedInterface> tested_instance;

		/**
		 * @brief The ID of the thread using this executor.
		 */
		u32 thread_id;

		/**
		 * @brief A reference to the history in which executed operations will be recorded.
		 */
		Ref<_History> history;
	};
}
