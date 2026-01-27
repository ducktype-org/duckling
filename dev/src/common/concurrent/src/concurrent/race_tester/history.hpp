#pragma once

#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <functional>
#include <variant>

namespace concurrent::tester {
	/**
	 * A thread-safe container for the history of operations performed on a concurrent data structure.
	 * @tparam TestedInterface The interface of the tested concurrent data structure.
	 * @tparam PossibleResults The possible result types of the operations.
	 * Use std::monostate instead of void.
	 */
	template<class TestedInterface, typename... PossibleResults>
	requires base::strConcatable<PossibleResults...> class History {
	public:
		using _ResultsVariant = std::variant<PossibleResults...>;

		/**
		 * @brief A recorded call event.
		 * @param operation A function object which can be called on any instance of the tested
		 * interface.
		 * @param description A human-readable description of the operation.
		 */
		struct Call {
			std::function<_ResultsVariant(Ref<TestedInterface>)> operation;
			std::string                                          description;

			[[nodiscard]]
			std::string toString() const {
				return "calls " + description;
			}
		};

		/**
		 * @brief A recorded return event.
		 * @param result The result of the operation, stored as a variant of possible result types.
		 */
		struct Return {
			_ResultsVariant result;

			[[nodiscard]]
			std::string toString() const {
				std::string result_str = "";
				VISIT(result, r, base::internal::strConcat(result_str, r));
				return "returns " + result_str;
			}
		};

		using Event = std::variant<Call, Return>;

		/**
		 * @brief A single history record, comprised of an event and the ID of the culprit thread.
		 */
		struct Record {
			u32   thread_id;
			Event event;

			[[nodiscard]]
			std::string toString() const {
				std::string event_string;
				variant_match(event) {
					variant_case(Call, call) { event_string = call.toString(); }
					variant_case(Return, ret) { event_string = ret.toString(); }
				}
				return "Thread " + base::strConcat(thread_id) + " " + event_string;
			}
		};

		void push_back(Record record) {
			std::lock_guard guard(mutex);
			records.push_back(std::move(record));
			num_threads = std::max(num_threads, record.thread_id + 1);
		}

		[[nodiscard]]
		usize size() const {
			return records.size();
		}

		[[nodiscard]]
		u32 getNumThreads() const {
			return num_threads;
		}

		[[nodiscard]]
		Record at(usize i) const {
			return records.at(i);
		}

		[[nodiscard]]
		std::string toString(usize upto = -1) const {
			std::string out = "History:";
			upto            = std::min(upto, records.size());
			for (usize i = 0; i < upto; ++i) {
				out += "\n";
				out += records.at(i).toString();
			}
			return out;
		}

	private:
		u32                 num_threads{ 0 };
		std::vector<Record> records{};
		std::mutex          mutex;
	};
}
