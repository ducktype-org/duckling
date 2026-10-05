// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "history.hpp"

#include <queue>

namespace concurrent::tester {
	/**
	 * A class which attempts to linearize a recorded concurrent history using a BFS approach.
	 * It can either confirm that a history is correct, or provide the shortest prefix of the
	 * history which is not linearizable.
	 *
	 * @tparam TestedInterface The interface that both implementations satisfy.
	 * @tparam SequentialImplementation The implementation type of the sequential class.
	 * It must be copy-constructible for the purpose of the linearizability search.
	 * @tparam PossibleResults The possible result types of the operations.
	 * Use std::monostate instead of void.
	 */
	template<
		class TestedInterface,
		std::derived_from<TestedInterface> SequentialImplementation,
		typename... PossibleResults>
	requires std::is_copy_constructible_v<SequentialImplementation> class BFSLinearizer {
	public:
		using History_        = History<TestedInterface, PossibleResults...>;
		using ResultsVariant_ = typename History_::ResultsVariant_;

		/**
		 * @brief The beginning and end indices of a single operation in the history.
		 */
		struct Op {
			usize begin, end;

			bool operator==(const Op& other) const = default;
		};

		/**
		 * @brief The state of the linearization search, i.e. a node in the implicit
		 * state graph which is being searched.
		 */
		struct State {
			/**
			 * @brief A list of "next operations" for each thread.
			 */
			std::vector<Op> next_ops;

			/**
			 * @brief The state of the sequential implementation.
			 */
			SequentialImplementation sequential_state;

			/**
			 * @brief The first index which is the end of one of `next_ops`.
			 */
			usize earliest_end;

			State(std::vector<Op> next_ops, SequentialImplementation sequential_state):
				  next_ops(std::move(next_ops)),
				  sequential_state(std::move(sequential_state)),
				  earliest_end(this->next_ops.at(0).end) {
				for (usize i = 1; i < this->next_ops.size(); ++i)
					earliest_end = std::min(earliest_end, this->next_ops.at(i).end);
			}
		};

		/**
		 * @brief Try to linearize the recorded history.
		 * @return None if linearization was successful, or the index of
		 * the first non-linearizable "return" record in the history.
		 */
		base::Optional<usize> linearize() {
			// Handle trivial case of empty history.
			if (history_size == 0) return {};

			std::queue<State> q{};
			q.push(getFirstState());
			// Tracks the earliest-ending operation which has not yet been linearized.
			usize max_earliest_end = q.front().earliest_end;

			// BFS loop, until we linearize all operations or exhaust the queue.
			while (max_earliest_end != -1 && not q.empty()) {
				Ref<State> current_state = &q.front();

				// Attempt to advance each thread.
				for (u32 thread_id = 0; thread_id < num_threads; ++thread_id) {
					// If the thread ran out of ops, skip it.
					if (current_state->next_ops.at(thread_id).begin == -1) continue;
					auto [next_state, result] = advance(current_state, thread_id);

					// Check if the operation's result matches the recorded one.
					const auto& event
						= history->at(current_state->next_ops.at(thread_id).end).event;
					const auto& recorded_result = std::get<typename History_::Return>(event).result;
					if (result != recorded_result) continue; /* Mismatch, discard this branch. */

					// If the results match, update max_earliest_end and add the state to the queue.
					max_earliest_end = std::max(max_earliest_end, next_state.earliest_end);
					q.push(std::move(next_state));
				}

				// Finally, remove the current state from the queue.
				q.pop();
			}

			// Return the index of the first non-linearizable return, if it exists.
			if (max_earliest_end != -1) return max_earliest_end;
			return {};
		}

		BFSLinearizer(CRef<History_> history, SequentialImplementation sequential_implementation):
			  history(history),
			  history_size(history->size()),
			  num_threads(history->getNumThreads()),
			  sequential_implementation(sequential_implementation) {}

	private:
		Op nextOpForThread(u32 thread_id, Op previous_op) {
			usize idx    = previous_op.end;
			Op    result = previous_op;
			// Search for the beginning of the next operation.
			for (++idx; idx < history_size; ++idx) {
				if (history->at(idx).thread_id == thread_id) {
					result.begin = idx;
					break;
				}
			}
			// Search for its end (which will be the next record from the same thread).
			for (++idx; idx < history_size; ++idx) {
				if (history->at(idx).thread_id == thread_id) {
					result.end = idx;
					break;
				}
			}

			if (result == previous_op) {
				// No more operations for this thread.
				return { -1ULL, -1ULL };
			}
			return result;
		}

		State getFirstState() {
			std::vector<Op> first_ops;
			first_ops.reserve(num_threads);
			for (u32 thread_id = 0; thread_id < num_threads; ++thread_id)
				first_ops.push_back(nextOpForThread(thread_id, Op{ -1ULL, -1ULL }));
			return State{ first_ops, sequential_implementation };
		}

		/**
		 * Advance the operation of a given thread, returning tits result and the next state.
		 * @param state The current state.
		 * @param thread_id The thread ID whose next operation to advance.
		 * @return The next state, and the result of the operation.
		 */
		std::pair<State, ResultsVariant_> advance(Ref<State> state, u32 thread_id) {
			// Get the next sequential state by executing the operation of the given thread.
			auto current_op     = state->next_ops.at(thread_id);
			auto next_seq_state = state->sequential_state;  // copy the state for modification.
			auto result = std::get<typename History_::Call>(history->at(current_op.begin).event)
			                  .operation(Ref(&next_seq_state));

			// Find the next operation for the given thread.
			auto next_op           = nextOpForThread(thread_id, current_op);
			auto next_ops          = state->next_ops;
			next_ops.at(thread_id) = next_op;

			return { State{ next_ops, std::move(next_seq_state) }, std::move(result) };
		}

		/**
		 * @brief The history to linearize.
		 */
		CRef<History_> history;

		/**
		 * @brief The size of the history, cached for readability.
		 */
		usize history_size;

		/**
		 * @brief The number of threads in the history, cached for readability.
		 */
		u32 num_threads;

		/**
		 * @brief The sequential implementation to check correctness of linearization.
		 * @note Must be copy-constructible.
		 */
		SequentialImplementation sequential_implementation;
	};
}
