#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vm/api/data/status.hpp>

template<typename T>
class BlockingQueue {
public:
	T pop() {
		std::unique_lock<std::mutex> mlock(mutex);
		while (queue.empty()) cond.wait(mlock);
		auto item = queue.front();
		queue.pop();
		return item;
	}

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
