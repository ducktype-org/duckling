#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace vm {

	/**
	 * @brief The process/API -> exec-thread control channel of a single VMThread.
	 *
	 * A `ThreadSignal` carries *requests* (Pause/Resume/Step/Stop).
	 *
	 * Request slot semantics:
	 * - `Stop` is sticky - once posted it is never overwritten and never cleared by `consume` -
	 * it stays pending, until `reset()` is called on thread reuse.
	 * - A non-Stop request lives in the slot until consumed. Posting a *different* non-Stop request
	 * while the slot is busy is rejected instead of overwriting.
	 */
	class ThreadSignal final {
	public:
		enum class Request : std::uint8_t { Pause, Resume, Step, Stop };

		[[nodiscard]] static constexpr std::string_view requestName(Request req) {
			switch (req) {
			case Request::Pause:
				return "Pause";
			case Request::Resume:
				return "Resume";
			case Request::Step:
				return "Step";
			case Request::Stop:
				return "Stop";
			default:
				CORE_UNREACHABLE();
			}
		}

		ThreadSignal() = default;

		ThreadSignal(const ThreadSignal&)            = delete;
		ThreadSignal& operator=(const ThreadSignal&) = delete;
		ThreadSignal(ThreadSignal&&)                 = delete;
		ThreadSignal& operator=(ThreadSignal&&)      = delete;

		/**
		 * @brief Posts a control request and wakes all waiters.
		 *
		 * @return true if the request was accepted (slot was empty, same request already pending,
		 * or `req == Stop`). False if the slot is busy with a different non-Stop request or a
		 * Stop is already pending (and we tried to post a different request than Stop).
		 */
		bool post(Request req) {
			{
				std::lock_guard lock(mutex);
				if (pending.has_value()) {
					if (*pending == Request::Stop) {
						// Stop was requested.
						if (req != Request::Stop) return false;
					} else if (req != Request::Stop && req != *pending) {
						// A different request was not yet consumed.
						return false;
					}
				}
				pending = req;
				refreshFlagsLocked();
			}
			cv.notify_all();
			return true;
		}

		/**
		 * @brief Takes the pending request, if it exists. Called by the exec thread at
		 * execution-break.
		 * @note `Stop` is returned but stays pending, see the file doc-comment.
		 */
		[[nodiscard]] base::Optional<Request> consume() {
			std::lock_guard lock(mutex);
			if (!pending.has_value()) return std::nullopt;
			return takePendingRequestLocked();
		}

		/**
		 * @brief Blocks until a request is pending and takes it.
		 * Called by the exec thread while its VMThread state is `Paused`.
		 * @note Callers must hold no other lock.
		 */
		[[nodiscard]] Request waitForRequest() {
			std::unique_lock lock(mutex);
			cv.wait(lock, [&] { return pending.has_value(); });
			return takePendingRequestLocked();
		}

		/**
		 * @brief true if any request is pending - i.e. a running interpreter loop should break
		 * execution and call `consume()`.
		 *
		 * @note Raised for every request kind on purpose: a Resume/Step that raced a state
		 * change landed on a *running* thread is consumed and dropped at the next
		 * execution boundary, so a stale request can never wedge the slot.
		 */
		[[nodiscard]] bool breakRequested() const { return break_flag.test(); }

		/**
		 * @brief true if a Stop is pending. Polled inside interruptible IO/mutex/CV
		 * waits and during unwind (global destructors etc.).
		 */
		[[nodiscard]] bool stopRequested() const { return stop_flag.test(); }

		/**
		 * @brief Wakes `waitInterruptible` waiters without posting a request. Used e.g. when
		 * API input arrives for a thread sleeping on an IO read.
		 */
		void notifyWaiters() { cv.notify_all(); }

		/**
		 * @brief Clears the slot and flags.
		 *
		 * Called by the `exec_thread` when its run ends, and by the spawner when preparing a
		 * re-spawn. Must only be called while the owning VMThread is non-active, so no
		 * request meant for a running thread can be dropped.
		 */
		void reset() {
			std::lock_guard lock(mutex);
			pending.reset();
			refreshFlagsLocked();
		}

		/**
		 * @brief Interruptible wait over a given lock (e.g. the process IO lock). Returns
		 * when `pred()` holds or a Stop was posted. The caller should check `stopRequested()` to
		 * distinguish the two.
		 *
		 * @note The waiter holds @p lock, not this signal's `mutex`, so a `post` can come after
		 * `pred()` was evaluated but before this thread actually started waiting on the CV. This
		 * request would then be missed. Thus, the wait re-checks every `INTERRUPT_POLL_INTERVAL`,
		 * so a missed request can still come delayed, but come eventually.
		 */
		template<class Lock, class Pred>
		void waitInterruptible(Lock& lock, Pred pred) {
			while (!cv.wait_for(lock, INTERRUPT_POLL_INTERVAL, [&] {
				return stopRequested() || pred();
			})) {}
		}

	private:
		static constexpr auto INTERRUPT_POLL_INTERVAL = std::chrono::milliseconds(100);

		static void setFlag(std::atomic_flag& flag, bool value) {
			if (value)
				flag.test_and_set();
			else
				flag.clear();
		}

		/**
		 * @brief Sets the atomic flags based on the pending request.
		 * @note Call under `mutex` after any change.
		 */
		void refreshFlagsLocked() {
			setFlag(break_flag, pending.has_value());
			setFlag(stop_flag, pending.has_value() && *pending == Request::Stop);
		}

		/**
		 * @brief Takes the pending request and refreshes the flags. A `Stop` always stays pending.
		 * @note Call under `mutex`, with a request pending.
		 */
		Request takePendingRequestLocked() {
			const Request req = *pending;
			if (req != Request::Stop) pending.reset();
			refreshFlagsLocked();
			return req;
		}

		std::mutex                  mutex;  ///< Main ThreadSignal lock.
		std::condition_variable_any cv;
		base::Optional<Request>     pending;

		std::atomic_flag break_flag;
		std::atomic_flag stop_flag;
	};
}
