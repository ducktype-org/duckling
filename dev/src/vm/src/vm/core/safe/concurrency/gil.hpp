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
		std::timed_mutex gil;

		/**
		 * @brief Count of how many times GIL was exchanged between threads.
		 */
		std::atomic<u64> exchange_counter{ 0 };

		/**
		 * @brief Time in milliseconds after which thread waiting for GIL will raise a flag
		 * to notify running thread to release GIL.
		 */
		static constexpr u64 TIMEOUT_MS = 5;

		/**
		 * @brief Flag which indicates that waiting thread is waiting for GIL for too long
		 * and running thread should release it.
		 */
		std::atomic<bool> release_requested_flag = false;


	public:
		~GIL();

		/**
		 * @brief Acquires GIL. After you call this function you always have right to interpret DVM
		 * code.
		 */
		void acquire();

		/**
		 * @brief Releases GIL. After you call this function you no longer can interpret DVM code.
		 * It also increments exchanges counter, so waiting threads can detect release.
		 */
		void release();

		/**
		 * @brief Decides whether current thread should give up GIL based on set GIL policy by
		 * checking release_requested_flag.
		 */
		bool shouldRelease();

		/**
		 * @brief RAII guard acquiring the GIL for the scope. For use outside exec threads
		 * (e.g. by SafeVMProcess), where SafeVMThread::ScopedGilGuard does not apply.
		 */
		class ScopedLock {
		public:
			explicit ScopedLock(GIL& gil): gil(gil) { gil.acquire(); }
			~ScopedLock() { gil.release(); }

			ScopedLock(const ScopedLock&)            = delete;
			ScopedLock& operator=(const ScopedLock&) = delete;

		private:
			GIL& gil;
		};
	};
}
