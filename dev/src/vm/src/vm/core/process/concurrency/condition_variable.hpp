
#include <base/types/ints.hpp>

#include <mutex>
#include <atomic>
#include <condition_variable>

namespace vm {

	class ConditionVariable {
	private:
		std::condition_variable_any cv;
		std::atomic<std::mutex*> bound_mutex { nullptr };
	public:
		void wait(std::mutex &mutex);
		void notifyOne();
		void notifyAll();		
	};
}