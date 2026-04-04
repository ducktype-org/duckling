#pragma once

#include <base/pointers/ref.hpp>

#include <functional>
#include <mutex>
#include <set>

namespace events {


	template<class Event>
	class Emitter;

	/**
	 * @brief A wrapper to function that is executed on emission
	 *
	 * @tparam Event Argument of the function
	 */
	template<class Event>
	class Listener {
		using Handler = std::function<void(const Event&)>;

		Handler                    handler;
		base::MRef<Emitter<Event>> emitter;

	public:
		explicit Listener(Handler&& handler): handler(std::move(handler)) {}

		// Disallow copy and move since Emitter stores raw pointers to Listeners
		Listener(const Listener&)            = delete;
		Listener& operator=(const Listener&) = delete;
		Listener(Listener&&)                 = delete;
		Listener& operator=(Listener&&)      = delete;

		/**
		 * @brief Returns true if Listener is attached to any Emitter
		 */
		bool isAttached() { return emitter; }

		/**
		 * @brief If Listener is attached to some Emitter then detaches from it
		 */
		void detach() {
			if (emitter) {
				auto copy = emitter;
				emitter   = nullptr;
				copy->detachListener(this);
			}
		}

		/**
		 * @brief Attach Listener to the Emitter
		 */
		void attach(Ref<Emitter<Event>> emitter) {
			CORE_ASSERT(!isAttached() || this->emitter == emitter, "Tried to reattach listener");
			if (!isAttached()) {
				this->emitter = emitter;
				emitter->attachListener(this);
			}
		}

		/**
		 * @brief Attach Listener to the Emitter
		 */
		void attach(Emitter<Event>& emitter) { attach(&emitter); }

		/**
		 * @brief Execute the wrapped function
		 */
		void handle(const Event& event) { handler(event); }

		~Listener() { detach(); }
	};

	/**
	 * @brief Simple class to propagate events using Listeners
	 *
	 * @tparam Event Argument of the functions
	 */
	template<class Event>
	class Emitter {
		std::set<Ref<Listener<Event>>> listeners{};

		std::recursive_mutex listeners_mutex;

	public:
		Emitter() = default;
		// Disallow copy and move since Listener stores raw pointer to Emitter
		Emitter(const Emitter&)            = delete;
		Emitter& operator=(const Emitter&) = delete;
		Emitter(Emitter&&)                 = delete;
		Emitter& operator=(Emitter&&)      = delete;

		/**
		 * @brief Attach Listener to the Emitter
		 */
		void attachListener(Ref<Listener<Event>> listener) {
			std::lock_guard<std::recursive_mutex> lock(listeners_mutex);
			listeners.insert(listener);
			listener->attach(this);
		}

		/**
		 * @brief Attach Listener to the Emitter
		 */
		void attachListener(Listener<Event>& listener) { attachListener(&listener); }

		/**
		 * @brief Detach Listener from the Emitter
		 */
		void detachListener(Ref<Listener<Event>> listener) {
			std::lock_guard<std::recursive_mutex> lock(listeners_mutex);
			listeners.erase(listener);
			listener->detach();
		}

		/**
		 * @brief Detach Listener from the Emitter
		 */
		void detachListener(Listener<Event>& listener) { detachListener(&listener); }

		/**
		 * @brief Emit event to all attached Listeners
		 */
		void emitEvent(const Event& event) {
			std::lock_guard<std::recursive_mutex> lock(listeners_mutex);
			// We copy here to prevent invalidating iterators, when handling an event detaches
			auto copy = listeners;
			for (const auto& listener: copy) listener->handle(event);
		}

		~Emitter() {
			std::lock_guard<std::recursive_mutex> lock(listeners_mutex);
			// We copy here to not invalidate the iterators when detaching listeners.
			auto copy = listeners;
			for (const auto& listener: copy) listener->detach();
		}
	};
}
