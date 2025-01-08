#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <api/data/status.hpp>

namespace vm {

	class ExecutionStatusQueue {
	public:
		api::ExecStatus pop() {
			std::unique_lock<std::mutex> mlock(mutex);
			while (queue.empty()) cond.wait(mlock);
			auto item = queue.front();
			queue.pop();
			return item;
		}

		void push(const api::ExecStatus& item) {
			std::unique_lock<std::mutex> mlock(mutex);
			status = item;
			queue.push(item);
			mlock.unlock();
			cond.notify_one();
		}

		api::ExecStatus getStatus() {
			std::unique_lock<std::mutex> mlock(mutex);
			return status;
		}

		void setStatus(const api::ExecStatus& new_status) {
			std::unique_lock<std::mutex> mlock(mutex);
			status = new_status;
		}

		void clear() {
			std::unique_lock<std::mutex> mlock(mutex);
			while (!queue.empty()) queue.pop();
		}

	private:
		std::queue<api::ExecStatus> queue;
		api::ExecStatus             status;
		std::mutex                  mutex;
		std::condition_variable     cond;
	};
}
