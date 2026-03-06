#pragma once

#include <base/types/ints.hpp>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <base/pointers/ref.hpp>

namespace vm {

	class ConditionVariable final{
	private:
		std::condition_variable_any cv;
		std::atomic<std::mutex*> bound_mutex { nullptr };
	public:
		void wait(base::Ref<std::mutex>& mutex);
		void notifyOne();
		void notifyAll();		
	};
}