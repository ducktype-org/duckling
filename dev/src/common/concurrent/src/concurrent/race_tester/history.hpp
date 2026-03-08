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
	class History {
	public:
		using ResultsVariant_ = std::variant<PossibleResults...>;

		/**
		 * @brief A recorded call event.
		 * @param operation A function object which can be called on any instance of the tested
		 * interface.
		 * @param description A human-readable description of the operation.
		 */
		struct Call {
			std::function<ResultsVariant_(Ref<TestedInterface>)> operation;
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
			ResultsVariant_ result;

			[[nodiscard]]
			std::string toString() const {
				using base::internal::strConcat;
				std::string result_str = "";
				VISIT(result, r, strConcat(result_str, r));
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

		void pushBack(Record record) {
			std::lock_guard guard(mutex);
			records.push_back(std::move(record));
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
		const Record& at(usize i) const {
			return records.at(i);
		}

		/**
		 * @param upto The index of the instruction up to which to print (inclusive).
		 * @return The stringified history.
		 */
		[[nodiscard]]
		std::string toString(usize upto = -2ULL) const {
			std::string out = "History:";
			upto            = std::min(upto + 1, records.size());
			for (usize i = 0; i < upto; ++i) {
				out += "\n" + std::to_string(i) + ": ";
				out += records.at(i).toString();
			}
			return out;
		}

		explicit History(const u32 num_threads): num_threads(num_threads) {}

	private:
		u32                 num_threads;
		std::vector<Record> records{};
		mutable std::mutex  mutex;
	};
}
