#pragma once

#include <base/types/ints.hpp>

#include <atomic>
#include <mutex>

namespace vm {

	class GIL final {
	private:
		/**
		 * @brief Main GIL mutex for a process.
		 * It stems from assumption that only one thread can be executing DVM code at the time.
		 */
		std::recursive_timed_mutex gil;

		/**
		 * @brief Count of how many times GIL was exchanged between threads.
		 */
		std::atomic<u64> gil_exchanges{ 0 };

		/**
		 * @brief Time in milliseconds after which thread waiting for GIL will raise a flag
		 * to notify running thread to release GIL.
		 */
		static constexpr u64 GIL_TIMEOUT_MS = 5;

		/**
		 * @brief Flag which indicates that waiting thread is waiting for GIL for too long
		 * and running thread should release it.
		 */
		std::atomic<bool> gil_timeout_flag = false;


	public:
		/**
		 * @brief Acquires GIL. After you call this function you always have right to interpret DVM
		 * code.
		 */
		void acquire();

		/**
		 * @brief Releases GIL. After you call this function you no longer can interpret DVM code.
		 * It also zeroes operations counter, for the next thread to take GIL.
		 */
		void release();

		/**
		* @brief Decides whether current thread should give up GIL based on set GIL policy.
		In the future it will have seprarte interace, for now it is simple counter.
		*/
		bool shouldRelease();
	};
}
