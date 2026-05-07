#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

template<typename T>
class BlockingQueue {
public:
	/**
	 * @brief Pops an item from the queue. If the queue is empty, it waits until an item is available.
	 */
	T pop() {
		std::unique_lock<std::mutex> mlock(mutex);
		while (queue.empty()) cond.wait(mlock);
		auto item = queue.front();
		queue.pop();
		return item;
	}

	/**
	 * @brief Pushes an item to the queue and notifies one waiting thread.
	 */
	void push(const T& item) {
		std::unique_lock<std::mutex> mlock(mutex);
		queue.push(item);
		mlock.unlock();
		cond.notify_one();
	}

private:
	std::queue<T>           queue;
	std::mutex              mutex;
	std::condition_variable cond;
};
