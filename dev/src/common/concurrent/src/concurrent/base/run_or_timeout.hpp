#pragma once

#include <base/types/ints.hpp>

#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>

namespace concurrent {
	template<class F, class... Args>
	concept JThreadStoppable = std::is_invocable_v<F, const std::stop_token&, Args...>;

	/**
	 * @brief Runs a function with a timeout. If the function does not complete within the specified
	 * timeout, the thread executing the function is stopped. If the func supports cooperative
	 * cancellation via `std::stop_token`, it will be requested to stop, otherwise the process will
	 * be forcefully terminated.
	 *
	 * @param func The function to run. The function can optionally take a `std::stop_token` as its
	 * first argument if it supports cooperative cancellation. Otherwise, if the function does not
	 * support cancellation, the process will be forcefully terminated on timeout.
	 * @param timeout_callback The callback to invoke if a timeout occurs. This can be used to
	 * perform any necessary cleanup before termination.
	 * @param timeout_ms The timeout duration in milliseconds. Default is 5000 ms (5 seconds).
	 */
	template<class F, class G>
	void runOrTimeout(F func, G timeout_callback, usize timeout_ms = 5'000) {
		std::mutex              mutex;
		std::condition_variable cv;
		std::atomic_bool        done = false;

		constexpr bool IS_STOPPABLE = JThreadStoppable<F>;
		std::jthread   worker_thread([&func, &cv, &done, &mutex](const std::stop_token& st) {
            if constexpr (IS_STOPPABLE)
                func(st);
            else
                func();
            std::scoped_lock lock(mutex);
            done.store(true, std::memory_order_relaxed);
            cv.notify_one();
        });

		{
			std::unique_lock lock(mutex);
			if (!done.load(std::memory_order_relaxed)
			    && cv.wait_for(lock, std::chrono::milliseconds(timeout_ms))
			           == std::cv_status::timeout) {
				if (IS_STOPPABLE)
					worker_thread.request_stop();
				else
					worker_thread.detach();  // Detach the worker thread since we are going to
					                         // terminate the process
				std::cerr << "Timeout after " << timeout_ms << " ms. Terminating the task.\n";
				timeout_callback();
				if (!IS_STOPPABLE) std::terminate();
			}
		}
	}
}
